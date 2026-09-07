/*
 * Garage Music patched AVRCP Cover Art service.
 * - Uses ESP-IDF Cover Art/BIP client
 * - Uses safe MTU 512 for ESP32 without PSRAM
 * - Decodes linked thumbnail JPEG to LVGL-native RGB565
 * - Pushes decoded album cover into SquareLine/LVGL ui_imgAlbum
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "jpeg_decoder.h"
#include "avrcp_common_utils.h"
#include "avrcp_cover_art_service.h"
#include "garage_lvgl.h"

#define RC_CA_SRV_TAG "RC_CA_SRV"

#define LCD_HOST SPI3_HOST
#define PARALLEL_LINES CONFIG_EXAMPLE_LCD_FLUSH_PARALLEL_LINES

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL   1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL  0

#define EXAMPLE_PIN_NUM_DATA0           23  /* MOSI */
#define EXAMPLE_PIN_NUM_PCLK            18  /* SCLK */
#define EXAMPLE_PIN_NUM_CS              5
#define EXAMPLE_PIN_NUM_DC              21
#define EXAMPLE_PIN_NUM_RST             22
#define EXAMPLE_PIN_NUM_BK_LIGHT        13

#define EXAMPLE_LCD_H_RES               320
#define EXAMPLE_LCD_V_RES               240
#define EXAMPLE_LCD_CMD_BITS            8
#define EXAMPLE_LCD_PARAM_BITS          8

/* JPEG decoder output dimensions are detected dynamically. */
typedef struct {
    bool connected;
    bool getting;
    uint8_t image_hdl_old[7];
    uint32_t image_size;
    uint8_t *image_data;
    bool image_final;
    uint16_t *pixels;
    uint16_t decoded_w;
    uint16_t decoded_h;
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
} avrc_cover_art_srv_cb_t;

static bool avrc_cover_art_srv_image_handle_check(uint8_t *image_handle, int len);
static void avrc_cover_art_srv_free_image_data(void);
static void avrc_cover_art_srv_init_display(void);
static void avrc_cover_art_srv_deinit_display(void);
static void avrc_cover_art_srv_free_pixels(void);
static esp_err_t avrc_cover_art_srv_decode_image(void);
static void avrc_cover_art_srv_display_image(void);

static avrc_cover_art_srv_cb_t s_avrc_cover_art_srv_cb;

static bool avrc_cover_art_srv_image_handle_check(uint8_t *image_handle, int len)
{
    if (len == 7 && memcmp(s_avrc_cover_art_srv_cb.image_hdl_old, image_handle, 7) != 0) {
        memcpy(s_avrc_cover_art_srv_cb.image_hdl_old, image_handle, 7);
        return true;
    }
    return false;
}

static void avrc_cover_art_srv_free_image_data(void)
{
    if (s_avrc_cover_art_srv_cb.image_data) {
        free(s_avrc_cover_art_srv_cb.image_data);
        s_avrc_cover_art_srv_cb.image_data = NULL;
    }
    s_avrc_cover_art_srv_cb.image_size = 0;
}

static void avrc_cover_art_srv_free_pixels(void)
{
    if (s_avrc_cover_art_srv_cb.pixels) {
        free(s_avrc_cover_art_srv_cb.pixels);
        s_avrc_cover_art_srv_cb.pixels = NULL;
    }
    s_avrc_cover_art_srv_cb.decoded_w = 0;
    s_avrc_cover_art_srv_cb.decoded_h = 0;
}

static void avrc_cover_art_srv_init_display(void)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_PCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_DATA0,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * EXAMPLE_LCD_H_RES * 2 + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config,
                                             &s_avrc_cover_art_srv_cb.io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_avrc_cover_art_srv_cb.io_handle,
                                             &panel_config,
                                             &s_avrc_cover_art_srv_cb.panel_handle));

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_avrc_cover_art_srv_cb.panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_avrc_cover_art_srv_cb.panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_avrc_cover_art_srv_cb.panel_handle, true));

    /* Same visible behavior as working Arduino/LovyanGFX config. */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_avrc_cover_art_srv_cb.panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_avrc_cover_art_srv_cb.panel_handle, true));

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL));

    ESP_LOGI(RC_CA_SRV_TAG, "LCD init done for LVGL: MOSI=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d",
             EXAMPLE_PIN_NUM_DATA0,
             EXAMPLE_PIN_NUM_PCLK,
             EXAMPLE_PIN_NUM_CS,
             EXAMPLE_PIN_NUM_DC,
             EXAMPLE_PIN_NUM_RST,
             EXAMPLE_PIN_NUM_BK_LIGHT);

    garage_lvgl_start(s_avrc_cover_art_srv_cb.panel_handle);
}

static void avrc_cover_art_srv_deinit_display(void)
{
    garage_lvgl_stop();

    if (s_avrc_cover_art_srv_cb.panel_handle == NULL && s_avrc_cover_art_srv_cb.io_handle == NULL) {
        return;
    }

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL));

    if (s_avrc_cover_art_srv_cb.panel_handle) {
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_avrc_cover_art_srv_cb.panel_handle, false));
        ESP_ERROR_CHECK(esp_lcd_panel_del(s_avrc_cover_art_srv_cb.panel_handle));
        s_avrc_cover_art_srv_cb.panel_handle = NULL;
    }

    if (s_avrc_cover_art_srv_cb.io_handle) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_del(s_avrc_cover_art_srv_cb.io_handle));
        s_avrc_cover_art_srv_cb.io_handle = NULL;
    }

    ESP_ERROR_CHECK(spi_bus_free(LCD_HOST));
    ESP_ERROR_CHECK(gpio_reset_pin(EXAMPLE_PIN_NUM_BK_LIGHT));
}

static esp_err_t avrc_cover_art_srv_decode_image(void)
{
    esp_err_t ret = ESP_OK;

    avrc_cover_art_srv_free_pixels();

    ESP_LOGI(RC_CA_SRV_TAG, "JPEG input size: %lu bytes", s_avrc_cover_art_srv_cb.image_size);

    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = (uint8_t *)s_avrc_cover_art_srv_cb.image_data,
        .indata_size = s_avrc_cover_art_srv_cb.image_size,
        .outbuf = NULL,
        .outbuf_size = 0,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_1_4,
        .flags = {
            /* LVGL and SquareLine export expect LV_COLOR_16_SWAP == 0. The LVGL flush callback swaps bytes for LCD. */
            .swap_color_bytes = false,
        },
    };

    esp_jpeg_image_output_t outimg = {0};

    ret = esp_jpeg_get_image_info(&jpeg_cfg, &outimg);
    if (ret != ESP_OK) {
        ESP_LOGE(RC_CA_SRV_TAG, "JPEG get image info failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(RC_CA_SRV_TAG,
             "JPEG scaled output: %d x %d, output_len: %d bytes",
             outimg.width,
             outimg.height,
             outimg.output_len);

    s_avrc_cover_art_srv_cb.pixels = calloc(1, outimg.output_len);
    ESP_GOTO_ON_FALSE(s_avrc_cover_art_srv_cb.pixels,
                      ESP_ERR_NO_MEM,
                      err,
                      RC_CA_SRV_TAG,
                      "Error allocating scaled RGB565 buffer");

    jpeg_cfg.outbuf = (uint8_t *)s_avrc_cover_art_srv_cb.pixels;
    jpeg_cfg.outbuf_size = outimg.output_len;

    ret = esp_jpeg_decode(&jpeg_cfg, &outimg);
    if (ret != ESP_OK) {
        ESP_LOGE(RC_CA_SRV_TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        goto err;
    }

    s_avrc_cover_art_srv_cb.decoded_w = outimg.width;
    s_avrc_cover_art_srv_cb.decoded_h = outimg.height;

    ESP_LOGI(RC_CA_SRV_TAG,
             "JPEG decoded OK for LVGL: %d x %d, %d bytes",
             outimg.width,
             outimg.height,
             outimg.output_len);

    return ESP_OK;

err:
    avrc_cover_art_srv_free_pixels();
    return ret;
}

static void avrc_cover_art_srv_display_image(void)
{
    if (s_avrc_cover_art_srv_cb.pixels &&
        s_avrc_cover_art_srv_cb.decoded_w &&
        s_avrc_cover_art_srv_cb.decoded_h) {

        ESP_LOGI(RC_CA_SRV_TAG, "Pushing cover to LVGL ui_imgAlbum: %ux%u",
                 s_avrc_cover_art_srv_cb.decoded_w,
                 s_avrc_cover_art_srv_cb.decoded_h);

        garage_ui_set_album_cover_rgb565(
            s_avrc_cover_art_srv_cb.pixels,
            s_avrc_cover_art_srv_cb.decoded_w,
            s_avrc_cover_art_srv_cb.decoded_h
        );

        avrc_cover_art_srv_free_pixels();
        avrc_cover_art_srv_free_image_data();
    }
}

void avrc_cover_art_srv_open(void)
{
    if (s_avrc_cover_art_srv_cb.panel_handle) {
        ESP_LOGI(RC_CA_SRV_TAG, "Cover art LCD/LVGL already open");
        return;
    }

    memset(&s_avrc_cover_art_srv_cb, 0, sizeof(avrc_cover_art_srv_cb_t));
    avrc_cover_art_srv_init_display();
    garage_ui_set_status("WAIT");
    ESP_LOGI(RC_CA_SRV_TAG, "Cover art LCD/LVGL opened at boot");
}

void avrc_cover_art_srv_close(void)
{
    /* Keep LCD/LVGL alive on BT disconnect. Only clear transfer state. */
    s_avrc_cover_art_srv_cb.connected = false;
    s_avrc_cover_art_srv_cb.getting = false;
    s_avrc_cover_art_srv_cb.image_final = false;
    avrc_cover_art_srv_free_image_data();
    avrc_cover_art_srv_free_pixels();
    garage_ui_set_status("WAIT");
    ESP_LOGI(RC_CA_SRV_TAG, "Cover art service closed, LCD/LVGL kept alive");
}

void avrc_cover_art_srv_connect(uint16_t mtu)
{
    if (!s_avrc_cover_art_srv_cb.connected) {
        uint16_t safe_mtu = 512;
        ESP_LOGW(RC_CA_SRV_TAG,
                 "Start cover art connection... requested mtu: %u, using safe mtu: %u",
                 mtu,
                 safe_mtu);
        esp_avrc_ct_cover_art_connect(safe_mtu);
    }
}

void avrc_cover_art_srv_set_image_final(bool final)
{
    s_avrc_cover_art_srv_cb.image_final = final;

    if (s_avrc_cover_art_srv_cb.image_final) {
        ESP_LOGI(RC_CA_SRV_TAG,
                 "Cover Art Client final data event, image size: %lu bytes",
                 s_avrc_cover_art_srv_cb.image_size);

        esp_err_t ret = avrc_cover_art_srv_decode_image();

        if (ret == ESP_OK) {
            ESP_LOGI(RC_CA_SRV_TAG, "Cover decoded OK, send to LVGL");
            avrc_cover_art_srv_display_image();
        } else {
            ESP_LOGE(RC_CA_SRV_TAG, "Cover decode failed");
        }

        s_avrc_cover_art_srv_cb.getting = false;
    }
}

void avrc_cover_art_srv_set_connected(bool connected)
{
    s_avrc_cover_art_srv_cb.connected = connected;
    garage_ui_set_status(connected ? "BT OK" : "WAIT");
}

void avrc_cover_art_srv_ca_req(void)
{
    if (s_avrc_cover_art_srv_cb.connected) {
        uint8_t attr_mask = ESP_AVRC_MD_ATTR_PLAYING_TIME | ESP_AVRC_MD_ATTR_COVER_ART;
        esp_avrc_ct_send_metadata_cmd(bt_avrc_common_alloc_tl(), attr_mask);
    }
}

void avrc_cover_art_srv_save_image_data(uint8_t *p_data, uint16_t data_len)
{
    if (!p_data || data_len == 0) return;

    uint32_t old_size = s_avrc_cover_art_srv_cb.image_size;
    uint32_t new_size = old_size + data_len;

    uint8_t *p_buf = (uint8_t *)realloc(s_avrc_cover_art_srv_cb.image_data, new_size);
    if (!p_buf) {
        ESP_LOGE(RC_CA_SRV_TAG, "%s: The memory allocation of Cover art image data failed", __func__);
        avrc_cover_art_srv_free_image_data();
        s_avrc_cover_art_srv_cb.getting = false;
        return;
    }

    s_avrc_cover_art_srv_cb.image_data = p_buf;
    memcpy(s_avrc_cover_art_srv_cb.image_data + old_size, p_data, data_len);
    s_avrc_cover_art_srv_cb.image_size = new_size;
}

void avrc_cover_art_srv_ct_metadata_update(uint8_t *image_handle, int len)
{
    if (s_avrc_cover_art_srv_cb.connected && !s_avrc_cover_art_srv_cb.getting) {
        if (avrc_cover_art_srv_image_handle_check(image_handle, len)) {
            avrc_cover_art_srv_free_image_data();
            avrc_cover_art_srv_free_pixels();
            s_avrc_cover_art_srv_cb.image_final = false;
            s_avrc_cover_art_srv_cb.getting = true;
            esp_avrc_ct_cover_art_get_linked_thumbnail(image_handle);
        }
    }
}




