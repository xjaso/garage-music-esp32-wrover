#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"

#include "lvgl.h"
#include "ui.h"
#include "garage_lvgl.h"

#define GARAGE_LCD_W 320
#define GARAGE_LCD_H 240
#define GARAGE_LVGL_BUF_LINES 10
#define GARAGE_ALBUM_W 76
#define GARAGE_ALBUM_H 76

/* The current ST7789/LVGL panel mapping from the ESP-IDF driver displays the
 * SquareLine UI 180 degrees rotated compared with the working Arduino/LovyanGFX
 * build. Rotate the flushed LVGL area in software so the UI matches the known
 * good Test(1).zip graphics. Set to 0 only if the image is already correct. */
#define GARAGE_ROTATE_180 1

static const char *TAG = "GARAGE_LVGL";

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t *s_buf1 = NULL;
static uint16_t *s_swap_buf = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;
static esp_timer_handle_t s_tick_timer = NULL;
static TaskHandle_t s_lvgl_task = NULL;
static bool s_started = false;

static uint16_t *s_album_pixels = NULL;
static lv_img_dsc_t s_album_dsc;

static uint16_t swap16(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(5);
}

static void garage_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    if (!s_panel || !s_swap_buf) {
        lv_disp_flush_ready(disp);
        return;
    }

    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    int32_t len = w * h;
    int32_t max_len = GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES;

    if (len > max_len) {
        ESP_LOGW(TAG, "flush area too large %ld > %ld", (long)len, (long)max_len);
        len = max_len;
    }

    const uint16_t *src = (const uint16_t *)color_p;

#if GARAGE_ROTATE_180
    /* Vertical flip only.
     * This keeps left/right normal, but flips top/bottom.
     * Use this when the image is no longer upside down but is still mirrored. */
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int32_t src_i = y * w + x;
            int32_t dst_i = (h - 1 - y) * w + x;
            s_swap_buf[dst_i] = swap16(src[src_i]);
        }
    }

    int32_t dx1 = area->x1;
    int32_t dy1 = GARAGE_LCD_H - 1 - area->y2;
    int32_t dx2 = area->x2 + 1;
    int32_t dy2 = GARAGE_LCD_H - area->y1;

    esp_lcd_panel_draw_bitmap(s_panel,
                              dx1,
                              dy1,
                              dx2,
                              dy2,
                              s_swap_buf);
#else
    for (int32_t i = 0; i < len; i++) {
        /* LVGL/SquareLine is LV_COLOR_16_SWAP=0. ST7789 over SPI needs high byte first. */
        s_swap_buf[i] = swap16(src[i]);
    }

    esp_lcd_panel_draw_bitmap(s_panel,
                              area->x1,
                              area->y1,
                              area->x2 + 1,
                              area->y2 + 1,
                              s_swap_buf);
#endif

    lv_disp_flush_ready(disp);
}

static void garage_lvgl_task(void *arg)
{
    (void)arg;
    while (s_started) {
        if (s_lvgl_mutex) xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
        lv_timer_handler();
        if (s_lvgl_mutex) xSemaphoreGive(s_lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_lvgl_task = NULL;
    vTaskDelete(NULL);
}

void garage_lvgl_start(esp_lcd_panel_handle_t panel_handle)
{
    if (s_started) return;

    s_panel = panel_handle;
    s_lvgl_mutex = xSemaphoreCreateMutex();

    s_buf1 = heap_caps_malloc(GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_swap_buf = heap_caps_malloc(GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!s_buf1 || !s_swap_buf || !s_lvgl_mutex) {
        ESP_LOGE(TAG, "LVGL RAM allocation failed");
        return;
    }

    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, NULL, GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = GARAGE_LCD_W;
    s_disp_drv.ver_res = GARAGE_LCD_H;
    s_disp_drv.flush_cb = garage_disp_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &s_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_tick_timer, 5 * 1000));

    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    ui_init();
    if (ui_lblBtStatus) {
        lv_label_set_text(ui_lblBtStatus, "BT");
    }
    xSemaphoreGive(s_lvgl_mutex);

    s_started = true;
    xTaskCreatePinnedToCore(garage_lvgl_task, "garage_lvgl", 4096, NULL, 3, &s_lvgl_task, 1);
    ESP_LOGI(TAG, "LVGL 8 UI started, orientation rotate180=%d", GARAGE_ROTATE_180);
}

void garage_lvgl_stop(void)
{
    s_started = false;
    if (s_tick_timer) {
        esp_timer_stop(s_tick_timer);
        esp_timer_delete(s_tick_timer);
        s_tick_timer = NULL;
    }
}

static void set_label(lv_obj_t *obj, const char *txt)
{
    if (!obj) return;
    lv_label_set_text(obj, txt && txt[0] ? txt : "");
}

void garage_ui_set_status(const char *txt)
{
    if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    set_label(ui_lblBtStatus, txt);
    xSemaphoreGive(s_lvgl_mutex);
}

void garage_ui_set_title(const char *txt)
{
    if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    set_label(ui_lblTitle, txt);
    xSemaphoreGive(s_lvgl_mutex);
}

void garage_ui_set_artist(const char *txt)
{
    if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    set_label(ui_lblArtist, txt);
    xSemaphoreGive(s_lvgl_mutex);
}

void garage_ui_set_album(const char *txt)
{
    if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    set_label(ui_lblAlbum, txt);
    xSemaphoreGive(s_lvgl_mutex);
}

static void format_time(char *out, size_t out_sz, uint32_t ms)
{
    uint32_t sec = ms / 1000;
    snprintf(out, out_sz, "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

void garage_ui_set_progress(uint32_t pos_ms, uint32_t duration_ms)
{
    if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
    if (ui_barProgress && duration_ms > 0) {
        uint32_t v = (pos_ms >= duration_ms) ? 1000 : (pos_ms * 1000UL) / duration_ms;
        lv_bar_set_value(ui_barProgress, (int32_t)v, LV_ANIM_OFF);
    }
    char now_buf[16], total_buf[16];
    format_time(now_buf, sizeof(now_buf), pos_ms);
    format_time(total_buf, sizeof(total_buf), duration_ms);
    set_label(ui_lblTimeNow, now_buf);
    set_label(ui_lblTimeTotal, total_buf);
    xSemaphoreGive(s_lvgl_mutex);
}

void garage_ui_set_album_cover_rgb565(const uint16_t *src_rgb565, uint16_t src_w, uint16_t src_h)
{
    if (!s_started || !s_lvgl_mutex || !src_rgb565 || src_w == 0 || src_h == 0) return;

    if (!s_album_pixels) {
        s_album_pixels = heap_caps_malloc(
            GARAGE_ALBUM_W * GARAGE_ALBUM_H * sizeof(uint16_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );

        if (!s_album_pixels) {
            ESP_LOGE(TAG, "No RAM for persistent LVGL album image");
            return;
        }

        ESP_LOGI(TAG, "Persistent LVGL album buffer allocated: %u bytes",
                 GARAGE_ALBUM_W * GARAGE_ALBUM_H * 2);
    }

    for (int y = 0; y < GARAGE_ALBUM_H; y++) {
        int sy = (y * src_h) / GARAGE_ALBUM_H;
        for (int x = 0; x < GARAGE_ALBUM_W; x++) {
            int sx = (x * src_w) / GARAGE_ALBUM_W;
            s_album_pixels[y * GARAGE_ALBUM_W + x] = src_rgb565[sy * src_w + sx];
        }
    }

    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);

    memset(&s_album_dsc, 0, sizeof(s_album_dsc));
    s_album_dsc.header.always_zero = 0;
    s_album_dsc.header.w = GARAGE_ALBUM_W;
    s_album_dsc.header.h = GARAGE_ALBUM_H;
    s_album_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_album_dsc.data_size = GARAGE_ALBUM_W * GARAGE_ALBUM_H * sizeof(uint16_t);
    s_album_dsc.data = (const uint8_t *)s_album_pixels;

    lv_img_cache_invalidate_src(&s_album_dsc);
    lv_img_set_src(ui_imgAlbum, &s_album_dsc);
    lv_obj_set_size(ui_imgAlbum, GARAGE_ALBUM_W, GARAGE_ALBUM_H);
    lv_obj_invalidate(ui_imgAlbum);

    xSemaphoreGive(s_lvgl_mutex);

    ESP_LOGI(TAG, "Album cover pushed to ui_imgAlbum: %ux%u -> 76x76", src_w, src_h);
}




