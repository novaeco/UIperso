#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool sd_mounted;
    bool touch_available;
    bool wifi_connected;
    char wifi_ssid[33];
} system_status_t;

void system_status_init(void);
void system_status_set_sd_mounted(bool mounted);
void system_status_set_touch_available(bool available);
void system_status_set_wifi(bool connected, const char *ssid);
void system_status_get(system_status_t *out_status);
const system_status_t *system_status_get_ref(void);

#ifdef __cplusplus
}
#endif

