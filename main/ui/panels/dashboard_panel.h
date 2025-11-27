#pragma once

#include "lvgl.h"

#include "system_status.h"
#include "ui/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *dashboard_panel_create(lv_obj_t *parent, const ui_theme_styles_t *theme, const system_status_t *status_ref);
void dashboard_panel_update(const system_status_t *status_ref);

#ifdef __cplusplus
}
#endif

