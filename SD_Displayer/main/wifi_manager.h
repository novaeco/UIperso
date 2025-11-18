#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
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
    bool configured;
    bool use_dhcp;
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gateway;
    wifi_state_t state;
    bool ntp_synced;
    bool has_ip;
    wifi_state_t state;
} wifi_config_runtime_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
wifi_config_runtime_t wifi_manager_get_state(void);
void wifi_manager_request_reconnect(void);
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password, bool use_dhcp, const esp_netif_ip_info_t *static_ip);
bool wifi_manager_is_configured(void);

#ifdef __cplusplus
}
#endif
