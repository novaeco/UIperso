#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t cpu_freq_mhz;
    uint32_t ram_free_kb;
    uint32_t psram_free_kb;
    uint64_t uptime_ms;
    uint32_t reboot_count;
    const char *fw_version;
    const char *lvgl_version;
    const char *idf_version;
} hw_diag_stats_t;

// Backlight 0-100%
void hw_backlight_set_level(uint8_t level);

// Réseau
esp_err_t hw_network_connect(const char *ssid, const char *password);
void hw_network_disconnect(void);

// SD card
void hw_sdcard_refresh_status(void);
bool hw_sdcard_is_mounted(void);

// Communication
void hw_comm_set_can_baudrate(uint32_t baudrate);
void hw_comm_set_rs485_baudrate(uint32_t baudrate);

// Diagnostics
void hw_diag_get_stats(hw_diag_stats_t *out);

// Tactile
void hw_touch_start_test(void);
void hw_touch_stop_test(void);
