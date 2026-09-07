#pragma once

#include <stdint.h>
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

void garage_lvgl_start(esp_lcd_panel_handle_t panel_handle);
void garage_lvgl_stop(void);

void garage_ui_set_status(const char *txt);
void garage_ui_set_title(const char *txt);
void garage_ui_set_artist(const char *txt);
void garage_ui_set_album(const char *txt);
void garage_ui_set_progress(uint32_t pos_ms, uint32_t duration_ms);

/* src_rgb565 must be LVGL-native RGB565 byte order: LV_COLOR_16_SWAP == 0. */
void garage_ui_set_album_cover_rgb565(const uint16_t *src_rgb565, uint16_t src_w, uint16_t src_h);

#ifdef __cplusplus
}
#endif


