#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_SCREEN_SPLASH = 0,
    UI_SCREEN_HOME,
    UI_SCREEN_NETWORK,
    UI_SCREEN_STORAGE,
    UI_SCREEN_COMM,
    UI_SCREEN_DIAGNOSTICS,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_TOUCH_TEST,
    UI_SCREEN_ABOUT
} ui_screen_id_t;

void app_ui_init_navigation(void);
void app_ui_show_screen(ui_screen_id_t id);
void app_ui_update_status_bar(void);
void app_ui_update_network_status(void);
void app_ui_update_storage_status(void);
void app_ui_update_comm_status(void);
void app_ui_update_diag_status(void);
void app_ui_handle_keyboard_show(lv_obj_t *ta);
void app_ui_handle_keyboard_hide(void);
void app_ui_apply_backlight_level(int32_t level);
void app_ui_apply_backlight_profile(const char *profile_name, uint8_t level);
