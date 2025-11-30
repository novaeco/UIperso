#pragma once

#include "lvgl.h"

#include "system_status.h"
#include "ui/theme.h"
#include "reptile_net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_connect_cb_t)(const reptile_wifi_credentials_t *creds);

lv_obj_t *settings_panel_create(lv_obj_t *parent,
                                const ui_theme_styles_t *theme,
                                const system_status_t *status_ref,
                                wifi_connect_cb_t cb,
                                reptile_wifi_credentials_t *creds);
void settings_panel_update(const system_status_t *status_ref, const reptile_wifi_credentials_t *creds);

#ifdef __cplusplus
}
#endif

