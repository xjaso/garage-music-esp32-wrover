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

/* Album window from the Arduino/SquareLine TEST3 layout. */
#define GARAGE_ALBUM_W 100
#define GARAGE_ALBUM_H 100

/* Current ESP-IDF ST7789 mapping needs Y flip in the LVGL flush path.
 * This matches the corrected orientation you already verified on the display. */
#define GARAGE_FLIP_Y 1

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

#if GARAGE_FLIP_Y
    /* Vertical flip only. Left/right remains normal. */
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

    esp_lcd_panel_draw_bitmap(s_panel, dx1, dy1, dx2, dy2, s_swap_buf);
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
        garage_ui_progress_tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_lvgl_task = NULL;
    vTaskDelete(NULL);
}

static const int32_t TEXT_SCROLL_SPEED = 12;

static void configure_squareline_ui(void)
{
    if (ui_imgBackground) {
        lv_obj_set_pos(ui_imgBackground, 0, 0);
        lv_obj_set_size(ui_imgBackground, 320, 240);
        lv_obj_clear_flag(ui_imgBackground, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (ui_imgAlbum) {
        lv_obj_set_pos(ui_imgAlbum, 2, 37);
        lv_obj_set_size(ui_imgAlbum, GARAGE_ALBUM_W, GARAGE_ALBUM_H);
        lv_obj_clear_flag(ui_imgAlbum, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (ui_lblBtStatus) {
        lv_obj_set_pos(ui_lblBtStatus, 214, 6);
        lv_obj_set_size(ui_lblBtStatus, 48, 18);
        lv_label_set_long_mode(ui_lblBtStatus, LV_LABEL_LONG_CLIP);
        lv_label_set_text(ui_lblBtStatus, "WAIT");
        lv_obj_set_style_text_font(ui_lblBtStatus, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblBtStatus, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblBtStatus, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_lblBtStatus, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_lblTitle) {
        lv_obj_set_pos(ui_lblTitle, 124, 31);
        lv_obj_set_size(ui_lblTitle, 196, 42);
        lv_label_set_long_mode(ui_lblTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(ui_lblTitle, TEXT_SCROLL_SPEED, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_lblTitle, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblTitle, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblTitle, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_lblTitle, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_lblArtist) {
        lv_obj_set_pos(ui_lblArtist, 124, 70);
        lv_obj_set_size(ui_lblArtist, 196, 30);
        lv_label_set_long_mode(ui_lblArtist, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(ui_lblArtist, TEXT_SCROLL_SPEED, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_lblArtist, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblArtist, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblArtist, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_lblArtist, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_lblAlbum) {
        lv_obj_set_pos(ui_lblAlbum, 124, 101);
        lv_obj_set_size(ui_lblAlbum, 196, 30);
        lv_label_set_long_mode(ui_lblAlbum, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(ui_lblAlbum, TEXT_SCROLL_SPEED, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_lblAlbum, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblAlbum, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblAlbum, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_lblAlbum, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_barProgress) {
        lv_obj_set_pos(ui_barProgress, 10, 152);
        lv_obj_set_size(ui_barProgress, 300, 20);
        lv_bar_set_range(ui_barProgress, 0, 1000);
        lv_bar_set_value(ui_barProgress, 0, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(0xE7E3D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_barProgress, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_barProgress, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_barProgress, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_barProgress, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(0x202020), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_barProgress, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(ui_barProgress, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    if (ui_lblTimeNow) {
        lv_obj_set_pos(ui_lblTimeNow, 10, 170);
        lv_obj_set_size(ui_lblTimeNow, 100, 34);
        lv_label_set_text(ui_lblTimeNow, "00:00");
        lv_obj_set_style_text_font(ui_lblTimeNow, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblTimeNow, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblTimeNow, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_lblTimeTotal) {
        lv_obj_set_pos(ui_lblTimeTotal, 210, 170);
        lv_obj_set_size(ui_lblTimeTotal, 100, 34);
        lv_label_set_text(ui_lblTimeTotal, "--:--");
        lv_obj_set_style_text_font(ui_lblTimeTotal, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblTimeTotal, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblTimeTotal, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_lblTimeTotal, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void garage_lvgl_start(esp_lcd_panel_handle_t panel_handle)
{
    if (s_started) return;

    s_panel = panel_handle;
    s_lvgl_mutex = xSemaphoreCreateMutex();

    s_buf1 = heap_caps_malloc(GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES * sizeof(lv_color_t),
                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_swap_buf = heap_caps_malloc(GARAGE_LCD_W * GARAGE_LVGL_BUF_LINES * sizeof(uint16_t),
                                  MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

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
    configure_squareline_ui();
    xSemaphoreGive(s_lvgl_mutex);

    s_started = true;
    xTaskCreatePinnedToCore(garage_lvgl_task, "garage_lvgl", 4096, NULL, 3, &s_lvgl_task, 1);
    ESP_LOGI(TAG, "LVGL 8 UI started, flip_y=%d, album=%dx%d", GARAGE_FLIP_Y, GARAGE_ALBUM_W, GARAGE_ALBUM_H);
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
    uint32_t min = sec / 60;
    sec %= 60;
    if (min > 99) min = 99;
    snprintf(out, out_sz, "%02lu:%02lu", (unsigned long)min, (unsigned long)sec);
}

void garage_ui_set_progress(uint32_t pos_ms, uint32_t duration_ms)
{
if (!s_started || !s_lvgl_mutex) return;
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);

    if (ui_barProgress) {
        uint32_t v = 0;
        if (duration_ms > 0) {
            v = (pos_ms >= duration_ms) ? 1000 : (pos_ms * 1000UL) / duration_ms;
        }
        lv_bar_set_value(ui_barProgress, (int32_t)v, LV_ANIM_OFF);
    }

    char now_buf[16], total_buf[16];
    format_time(now_buf, sizeof(now_buf), pos_ms);
    if (duration_ms > 0) {
        format_time(total_buf, sizeof(total_buf), duration_ms);
    } else {
        snprintf(total_buf, sizeof(total_buf), "--:--");
    }
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
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!s_album_pixels) {
        ESP_LOGW(TAG, "No PSRAM for album buffer, fallback to internal RAM");

        s_album_pixels = heap_caps_malloc(
            GARAGE_ALBUM_W * GARAGE_ALBUM_H * sizeof(uint16_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );
    }

    if (!s_album_pixels) {
        ESP_LOGE(TAG, "No RAM for persistent LVGL album image");
        return;
    }

    ESP_LOGI(TAG, "Persistent LVGL album buffer allocated: %u bytes",
             (unsigned)(GARAGE_ALBUM_W * GARAGE_ALBUM_H * 2));
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
    lv_obj_set_pos(ui_imgAlbum, 2, 37);
    lv_obj_set_size(ui_imgAlbum, GARAGE_ALBUM_W, GARAGE_ALBUM_H);
    lv_obj_invalidate(ui_imgAlbum);

    xSemaphoreGive(s_lvgl_mutex);

    ESP_LOGI(TAG, "Album cover pushed to ui_imgAlbum: %ux%u -> %dx%d", src_w, src_h, GARAGE_ALBUM_W, GARAGE_ALBUM_H);
}


/* ---- AVRCP progress bridge ----
 * Android/AVRCP does not redraw LVGL by itself. These functions receive
 * duration, play position and playback state from avrcp_common_utils.c.
 */
static uint32_t s_ui_track_duration_ms = 0;
static uint32_t s_ui_track_position_ms = 0;
static uint32_t s_ui_last_progress_ms = 0;
static bool s_ui_progress_running = false;

void garage_ui_track_changed(void)
{
    s_ui_track_position_ms = 0;
    s_ui_last_progress_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_ui_progress_running = true;
    garage_ui_set_progress(s_ui_track_position_ms, s_ui_track_duration_ms);
}

void garage_ui_set_track_duration(uint32_t duration_ms)
{
    if (duration_ms == 0xFFFFFFFFUL) return;

    s_ui_track_duration_ms = duration_ms;

    if (s_ui_track_duration_ms > 0 && s_ui_track_position_ms > s_ui_track_duration_ms) {
        s_ui_track_position_ms = s_ui_track_duration_ms;
    }

    garage_ui_set_progress(s_ui_track_position_ms, s_ui_track_duration_ms);
}

void garage_ui_set_track_position(uint32_t position_ms)
{
    if (position_ms == 0xFFFFFFFFUL) return;

    if (s_ui_track_duration_ms > 0 && position_ms > s_ui_track_duration_ms) {
        position_ms = s_ui_track_duration_ms;
    }

    s_ui_track_position_ms = position_ms;
    s_ui_last_progress_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_ui_progress_running = true;

    garage_ui_set_progress(s_ui_track_position_ms, s_ui_track_duration_ms);
}

void garage_ui_set_playback_status_raw(uint8_t playback)
{
    /* ESP AVRCP playback status:
     * 0 = stopped, 1 = playing, 2 = paused, 3/4 = seek, 255 = error/unknown
     */
    switch (playback) {
    case 0x01:
        s_ui_progress_running = true;
        s_ui_last_progress_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        garage_ui_set_status("PLAY");
        break;

    case 0x02:
        s_ui_progress_running = false;
        garage_ui_set_status("PAUSE");
        break;

    case 0x00:
        s_ui_progress_running = false;
        garage_ui_set_status("STOP");
        break;

    case 0x03:
    case 0x04:
        s_ui_progress_running = false;
        garage_ui_set_status("SEEK");
        break;

    default:
        s_ui_progress_running = false;
        garage_ui_set_status("BT");
        break;
    }

    garage_ui_set_progress(s_ui_track_position_ms, s_ui_track_duration_ms);
}

void garage_ui_progress_tick(void)
{
    if (!s_ui_progress_running) return;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (s_ui_last_progress_ms == 0) {
        s_ui_last_progress_ms = now_ms;
        return;
    }

    uint32_t dt = now_ms - s_ui_last_progress_ms;

    /* update about 5x per second */
    if (dt < 200) return;

    s_ui_last_progress_ms = now_ms;
    s_ui_track_position_ms += dt;

    if (s_ui_track_duration_ms > 0 && s_ui_track_position_ms >= s_ui_track_duration_ms) {
        s_ui_track_position_ms = s_ui_track_duration_ms;
        s_ui_progress_running = false;
        garage_ui_set_status("STOP");
    }

    garage_ui_set_progress(s_ui_track_position_ms, s_ui_track_duration_ms);
}



