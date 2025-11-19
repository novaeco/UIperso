#pragma once

#include "lvgl.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

void rgb_lcd_init(void);
lv_display_t *rgb_lcd_get_disp(void);
esp_lcd_panel_handle_t rgb_lcd_get_panel(void);

#ifdef __cplusplus
}
#endif

