#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>

#define WIFI_DEFAULT_SSID "NovaEco_DEV"
#define WIFI_DEFAULT_PASSWORD "changeme1234"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
} wifi_state_t;

typedef struct {
    char ssid[33];
    char password[65];
    wifi_state_t state;
} wifi_config_runtime_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
wifi_config_runtime_t wifi_manager_get_state(void);
void wifi_manager_request_reconnect(void);

#ifdef __cplusplus
}
#endif
