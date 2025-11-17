#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t cpu_freq_mhz;
    uint32_t heap_free_bytes;
    uint32_t psram_free_bytes;
    uint32_t uptime_s;
    uint32_t reboot_counter;
} hw_diag_stats_t;

void hw_backlight_set_level(uint8_t level);
void hw_backlight_set_profile(const char *profile_name, uint8_t level);
esp_err_t hw_network_connect(const char *ssid, const char *password);
void hw_network_disconnect(void);
bool hw_network_is_connected(void);
int hw_network_get_rssi(void);
void hw_sdcard_refresh_status(void);
bool hw_sdcard_is_mounted(void);
uint32_t hw_sdcard_used_kb(void);
uint32_t hw_sdcard_total_kb(void);
void hw_comm_set_can_baudrate(uint32_t baudrate);
void hw_comm_set_rs485_baudrate(uint32_t baudrate);
void hw_diag_get_stats(hw_diag_stats_t *out);
void hw_diag_increment_reboot_counter(void);
