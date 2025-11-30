#pragma once

#include "lvgl.h"

#include "reptile_core.h"
#include "ui/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *logs_panel_view_create(lv_obj_t *parent, const ui_theme_styles_t *theme, reptiles_data_t *data);
void logs_panel_view_update(reptiles_data_t *data);

#ifdef __cplusplus
}
#endif
