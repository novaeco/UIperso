#pragma once
#include "lvgl.h"
#include "esp_lcd_panel_handle.h"
#include "esp_lcd_touch.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_display_t *lvgl_port_init(esp_lcd_panel_handle_t panel_handle, esp_lcd_touch_handle_t touch_handle);
void lvgl_port_register_fs(void);
bool lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
