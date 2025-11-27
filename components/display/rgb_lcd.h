#pragma once

#include "lvgl.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

void rgb_lcd_init(void);
lv_display_t *rgb_lcd_get_disp(void);
esp_lcd_panel_handle_t rgb_lcd_get_panel(void);
uint32_t rgb_lcd_flush_count_get_and_reset(void);
uint32_t rgb_lcd_flush_count_get(void);
void rgb_lcd_draw_test_pattern(void);

#ifdef __cplusplus
}
#endif

