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

void garage_ui_set_album_cover_rgb565(const uint16_t *src_rgb565, uint16_t src_w, uint16_t src_h);

/* AVRCP progress/status bridge */
void garage_ui_track_changed(void);
void garage_ui_set_track_duration(uint32_t duration_ms);
void garage_ui_set_track_position(uint32_t position_ms);
void garage_ui_set_playback_status_raw(uint8_t playback);
void garage_ui_progress_tick(void);

#ifdef __cplusplus
}
#endif


