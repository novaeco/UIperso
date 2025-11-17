#include "app_hw.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "app_hw";
static uint32_t reboot_counter = 0;
static uint8_t current_backlight = 80;
static bool network_connected = false;

void hw_backlight_set_level(uint8_t level)
{
    if (level > 100) level = 100;
    current_backlight = level;
    ESP_LOGI(TAG, "Backlight niveau %u%%", (unsigned)level);
}

void hw_backlight_set_profile(const char *profile_name, uint8_t level)
{
    ESP_LOGI(TAG, "Profil backlight %s -> %u%%", profile_name, (unsigned)level);
    hw_backlight_set_level(level);
}

esp_err_t hw_network_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connexion Wi-Fi SSID='%s'", ssid);
    network_connected = true;
    (void)password;
    return ESP_OK;
}

void hw_network_disconnect(void)
{
    ESP_LOGI(TAG, "Déconnexion Wi-Fi");
    network_connected = false;
}

bool hw_network_is_connected(void)
{
    return network_connected;
}

int hw_network_get_rssi(void)
{
    return network_connected ? -48 : -120;
}

void hw_sdcard_refresh_status(void)
{
    ESP_LOGI(TAG, "Rafraîchissement statut microSD (stub)");
}

bool hw_sdcard_is_mounted(void)
{
    return true;
}

uint32_t hw_sdcard_used_kb(void)
{
    return 128 * 1024; // 128 MB en KB
}

uint32_t hw_sdcard_total_kb(void)
{
    return 512 * 1024; // 512 MB en KB
}

void hw_comm_set_can_baudrate(uint32_t baudrate)
{
    ESP_LOGI(TAG, "CAN baudrate %lu", (unsigned long)baudrate);
}

void hw_comm_set_rs485_baudrate(uint32_t baudrate)
{
    ESP_LOGI(TAG, "RS485 baudrate %lu", (unsigned long)baudrate);
}

void hw_diag_get_stats(hw_diag_stats_t *out)
{
    if (!out) return;
    out->cpu_freq_mhz = 240;
    out->heap_free_bytes = esp_get_free_heap_size();
    out->psram_free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    out->reboot_counter = reboot_counter;
}

void hw_diag_increment_reboot_counter(void)
{
    reboot_counter++;
}
