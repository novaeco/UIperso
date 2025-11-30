#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char password[65];
} reptile_wifi_credentials_t;

typedef struct {
    bool connected;
    esp_ip4_addr_t ip;
} reptile_wifi_state_t;

typedef struct {
    const char *server;
    const char *tz;
} reptile_ntp_config_t;

esp_err_t reptile_net_prefs_init(void);
esp_err_t reptile_net_prefs_save_wifi(const reptile_wifi_credentials_t *creds);
esp_err_t reptile_net_prefs_load_wifi(reptile_wifi_credentials_t *creds);

esp_err_t reptile_net_wifi_start(const reptile_wifi_credentials_t *creds, reptile_wifi_state_t *state_out);

esp_err_t reptile_net_ntp_start(const reptile_ntp_config_t *config);

#ifdef __cplusplus
}
#endif

