#pragma once

#include "esp_err.h"
#include "lvgl.h"

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UI_MODE_NORMAL = 0,
    UI_MODE_DEGRADED_TOUCH,
    UI_MODE_DEGRADED_SD,
} ui_mode_t;

esp_err_t ui_manager_init(lv_display_t *disp, const system_status_t *status_ref);
void ui_manager_set_mode(ui_mode_t mode);
void ui_manager_tick_1s(void);

#ifdef __cplusplus
}
#endif

