#pragma once

#include "esp_err.h"
#include "lvgl.h"

#include "system_status.h"
#include "reptile_core.h"
#include "reptile_net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UI_MODE_NORMAL = 0,
    UI_MODE_DEGRADED_TOUCH,
    UI_MODE_DEGRADED_SD,
} ui_mode_t;

typedef struct
{
    const char *storage_mount;
    reptiles_data_t *reptiles;
    const reptile_wifi_state_t *wifi_state;
    reptile_wifi_credentials_t *wifi_creds;
    void (*save_data_cb)(const char *audit);
    void (*audit_cb)(const char *audit);
    void (*wifi_connect_cb)(const reptile_wifi_credentials_t *creds);
} ui_manager_ctx_t;

esp_err_t ui_manager_init(lv_display_t *disp, const system_status_t *status_ref, const ui_manager_ctx_t *ctx);
void ui_manager_set_mode(ui_mode_t mode);
void ui_manager_tick_1s(void);

#ifdef __cplusplus
}
#endif

