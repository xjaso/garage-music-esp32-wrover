#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define GARAGE_UI_BG_W 320
#define GARAGE_UI_BG_H 240

esp_err_t garage_ui_bg_draw_background(esp_lcd_panel_handle_t panel);


