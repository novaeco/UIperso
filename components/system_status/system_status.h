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
    bool can_ok;
    uint32_t can_frames_rx;

    bool rs485_ok;
    uint32_t rs485_tx_count;
    uint32_t rs485_rx_count;

    bool power_ok;
    bool power_telemetry_available;
    float vbat;
    bool charging_known;
    bool charging;
} system_status_t;

void system_status_init(void);
void system_status_set_can_ok(bool ok);
void system_status_increment_can_frames(void);
void system_status_set_rs485_ok(bool ok);
void system_status_add_rs485_tx(size_t bytes);
void system_status_add_rs485_rx(size_t bytes);
void system_status_set_power(bool ok, bool telemetry_available, float vbat, bool charging_known, bool charging);
void system_status_get(system_status_t *out_status);

#ifdef __cplusplus
}
#endif

