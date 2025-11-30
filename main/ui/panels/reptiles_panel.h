#pragma once

#include "lvgl.h"

#include "reptile_core.h"
#include "ui/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*reptile_save_cb_t)(const char *audit);

lv_obj_t *reptiles_panel_create(lv_obj_t *parent,
                                const ui_theme_styles_t *theme,
                                reptiles_data_t *data,
                                reptile_save_cb_t save_cb);
void reptiles_panel_update(const reptiles_data_t *data);

#ifdef __cplusplus
}
#endif

