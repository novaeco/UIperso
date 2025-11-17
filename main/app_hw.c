#include "app_hw.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/twai.h"
#include "esp_heap_caps.h"
#include "esp_clk.h"
#include "esp_system.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_hw";
static uint64_t s_boot_time_ms;
static uint32_t s_reboot_count;
static bool s_wifi_started = false;
static bool s_sd_mounted = false;
static sdmmc_card_t *s_card = NULL;
static esp_netif_t *s_netif = NULL;
static bool s_backlight_init = false;
static bool s_twai_started = false;
static bool s_touch_test_active = false;

// Backlight PWM sur LEDC channel 0 (GPIO configurable via menuconfig)
#define BACKLIGHT_GPIO    2
#define BACKLIGHT_TIMER   LEDC_TIMER_0
#define BACKLIGHT_MODE    LEDC_LOW_SPEED_MODE
#define BACKLIGHT_CHANNEL LEDC_CHANNEL_0

static void ensure_backlight_timer(void) {
    if (s_backlight_init) return;
    ledc_timer_config_t tcfg = {
        .speed_mode = BACKLIGHT_MODE,
        .timer_num = BACKLIGHT_TIMER,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));
    ledc_channel_config_t cconf = {
        .gpio_num = BACKLIGHT_GPIO,
        .speed_mode = BACKLIGHT_MODE,
        .channel = BACKLIGHT_CHANNEL,
        .timer_sel = BACKLIGHT_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = {.output_invert = 0},
    };
    ESP_ERROR_CHECK(ledc_channel_config(&cconf));
    s_backlight_init = true;
}

void hw_backlight_set_level(uint8_t level) {
    ensure_backlight_timer();
    uint32_t duty = (level * ((1 << 12) - 1)) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(BACKLIGHT_MODE, BACKLIGHT_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BACKLIGHT_MODE, BACKLIGHT_CHANNEL));
    app_ui_light_state_t light = {.backlight_level = level, .night_mode = level < 30};
    app_ui_set_light_state(&light);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    app_ui_wifi_state_t wifi_state = {.connected = false, .rssi = -120};
    strncpy(wifi_state.ip, "0.0.0.0", sizeof(wifi_state.ip));
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi déconnecté");
        app_ui_set_wifi_state(&wifi_state);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        wifi_state.connected = true;
        snprintf(wifi_state.ip, sizeof(wifi_state.ip), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_state.rssi = -55;
        app_ui_set_wifi_state(&wifi_state);
        ESP_LOGI(TAG, "Wi-Fi connecté: %s", wifi_state.ip);
    }
}

static void ensure_wifi(void) {
    if (s_wifi_started) return;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    s_wifi_started = true;
}

esp_err_t hw_network_connect(const char *ssid, const char *password) {
    ensure_wifi();
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_err_t err = esp_wifi_connect();
    return err;
}

void hw_network_disconnect(void) {
    if (!s_wifi_started) return;
    ESP_LOGI(TAG, "Demande déconnexion Wi-Fi");
    esp_wifi_disconnect();
}

void hw_sdcard_refresh_status(void) {
    if (s_sd_mounted) return;
    ESP_LOGI(TAG, "Montage microSD...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_config, &s_card);
    if (ret == ESP_OK) {
        s_sd_mounted = true;
    }
    app_ui_set_sd_state(s_sd_mounted);
}

bool hw_sdcard_is_mounted(void) {
    return s_sd_mounted;
}

void hw_comm_set_can_baudrate(uint32_t baudrate) {
    if (!s_twai_started) {
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)1, (gpio_num_t)3, TWAI_MODE_NORMAL);
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
        ESP_ERROR_CHECK(twai_start());
        s_twai_started = true;
    }
    ESP_LOGI(TAG, "CAN baudrate placeholder %u", baudrate);
}

void hw_comm_set_rs485_baudrate(uint32_t baudrate) {
    uart_config_t cfg = {
        .baud_rate = (int)baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
    ESP_LOGI(TAG, "RS485 baudrate réglé à %u", baudrate);
}

void hw_diag_get_stats(hw_diag_stats_t *out) {
    if (!out) return;
    out->cpu_freq_mhz = esp_clk_cpu_freq() / 1000000UL;
    out->ram_free_kb = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
    out->psram_free_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
    uint64_t now = esp_timer_get_time() / 1000ULL;
    out->uptime_ms = now - s_boot_time_ms;
    out->reboot_count = s_reboot_count;
    out->fw_version = "v0.1.0";
    out->lvgl_version = LVGL_VERSION_STRING;
    out->idf_version = esp_get_idf_version();
}

void hw_touch_start_test(void) {
    s_touch_test_active = true;
    ESP_LOGI(TAG, "Touch test start");
}

void hw_touch_stop_test(void) {
    if (s_touch_test_active) {
        ESP_LOGI(TAG, "Touch test stop");
    }
    s_touch_test_active = false;
}

static void __attribute__((constructor)) hw_boot_time_init(void) {
    s_boot_time_ms = esp_timer_get_time() / 1000ULL;
    s_reboot_count = 0; // TODO: charger depuis NVS
}
