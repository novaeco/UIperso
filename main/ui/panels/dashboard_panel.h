#pragma once

#include "lvgl.h"

#include "system_status.h"
#include "ui/theme.h"
#include "reptile_core.h"
#include "reptile_net.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *dashboard_panel_create(lv_obj_t *parent,
                                 const ui_theme_styles_t *theme,
                                 const system_status_t *status_ref,
                                 const reptiles_data_t *reptiles,
                                 const reptile_wifi_state_t *wifi_state,
                                 const char *storage_mount);
void dashboard_panel_update(const system_status_t *status_ref, const reptiles_data_t *reptiles, const reptile_wifi_state_t *wifi_state);

#ifdef __cplusplus
}
#endif

