#include "app_hw.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

static const char *TAG = "app_hw_stub";
static uint64_t s_boot_time_ms;
static uint32_t s_reboot_count;

void hw_backlight_set_level(uint8_t level) {
    ESP_LOGI(TAG, "Backlight set to %u%%", level);
    // TODO: implémenter contrôle PWM/backlight
}

esp_err_t hw_network_connect(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Connecting to SSID:%s", ssid ? ssid : "");
    // TODO: implémenter Wi-Fi station + sécurité
    return ESP_OK;
}

void hw_network_disconnect(void) {
    ESP_LOGI(TAG, "Wi-Fi disconnect");
    // TODO: implémenter déconnexion Wi-Fi
}

void hw_sdcard_refresh_status(void) {
    ESP_LOGI(TAG, "Refreshing SD status");
    // TODO: implémenter détection/montage SD
}

bool hw_sdcard_is_mounted(void) {
    // TODO: retourner l'état réel
    return false;
}

void hw_comm_set_can_baudrate(uint32_t baudrate) {
    ESP_LOGI(TAG, "Set CAN baudrate: %u", baudrate);
    // TODO: implémenter driver CAN
}

void hw_comm_set_rs485_baudrate(uint32_t baudrate) {
    ESP_LOGI(TAG, "Set RS485 baudrate: %u", baudrate);
    // TODO: implémenter driver RS485
}

void hw_diag_get_stats(hw_diag_stats_t *out) {
    if (!out) return;
    out->cpu_freq_mhz = 240; // TODO: récupérer fréquence réelle
    out->ram_free_kb = 0;    // TODO: heap_caps_get_free_size
    out->psram_free_kb = 0;  // TODO: heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
    uint64_t now = esp_timer_get_time() / 1000ULL;
    out->uptime_ms = now - s_boot_time_ms;
    out->reboot_count = s_reboot_count;
    out->fw_version = "v0.1.0";
    out->lvgl_version = LVGL_VERSION_STRING;
    out->idf_version = esp_get_idf_version();
}

void hw_touch_start_test(void) {
    ESP_LOGI(TAG, "Touch test start");
    // TODO: configurer callback de test
}

void hw_touch_stop_test(void) {
    ESP_LOGI(TAG, "Touch test stop");
    // TODO: arrêter test tactile
}

static void __attribute__((constructor)) hw_boot_time_init(void) {
    s_boot_time_ms = esp_timer_get_time() / 1000ULL;
    s_reboot_count = 0; // TODO: charger depuis NVS
}
