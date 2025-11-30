#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_STATE_OFFLINE = 0,
    NET_STATE_CONNECTING,
    NET_STATE_GOT_IP,
} net_state_t;

/**
 * Init NVS + netif + event loop + WiFi STA (si credentials présents en NVS),
 * puis lance SNTP quand une IP est obtenue.
 *
 * NVS: namespace="wifi", keys="ssid" et "pass"
 */
esp_err_t net_init(void);

net_state_t net_get_state(void);
bool net_is_time_synced(void);

#ifdef __cplusplus
}
#endif
