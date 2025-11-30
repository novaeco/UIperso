#include "net.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"

#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "net";

#define WIFI_NAMESPACE "wifi"
#define WIFI_KEY_SSID  "ssid"
#define WIFI_KEY_PASS  "pass"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_evt = NULL;
static int s_retry = 0;

static net_state_t s_state = NET_STATE_OFFLINE;
static bool s_time_synced = false;
static bool s_sntp_started = false;

static esp_err_t nvs_load_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len, bool *have)
{
    *have = false;

    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t ssid_req = ssid_len;
    size_t pass_req = pass_len;

    err = nvs_get_str(h, WIFI_KEY_SSID, ssid, &ssid_req);
    if (err != ESP_OK) { nvs_close(h); return err; }

    err = nvs_get_str(h, WIFI_KEY_PASS, pass, &pass_req);
    if (err != ESP_OK) { nvs_close(h); return err; }

    nvs_close(h);
    *have = (ssid[0] != '\0');
    return ESP_OK;
}

static void time_sync_cb(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
    ESP_LOGI(TAG, "Time sync completed.");
}

static void sntp_start_once(void)
{
    if (s_sntp_started) return;

    // Timezone France (CET/CEST)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = time_sync_cb;
    cfg.wait_for_sync = true;
    cfg.start = true;

    ESP_ERROR_CHECK(esp_netif_sntp_init(&cfg));
    s_sntp_started = true;

    esp_err_t r = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "SNTP sync wait failed/timeout (%s).", esp_err_to_name(r));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_state = NET_STATE_CONNECTING;
        esp_wifi_connect();
        return;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < 10) {
            s_retry++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/10...", s_retry);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_evt, WIFI_FAIL_BIT);
            s_state = NET_STATE_OFFLINE;
        }
        return;
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry = 0;
        s_state = NET_STATE_GOT_IP;
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);

        ESP_LOGI(TAG, "Got IP, starting SNTP...");
        sntp_start_once();
        return;
    }
}

static esp_err_t wifi_sta_start(const char *ssid, const char *pass)
{
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    s_wifi_evt = xEventGroupCreate();
    if (!s_wifi_evt) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    (void)esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t cfg = {0};
    snprintf((char *)cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid);
    snprintf((char *)cfg.sta.password, sizeof(cfg.sta.password), "%s", (pass ? pass : ""));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t net_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    char ssid[33] = {0};
    char pass[65] = {0};
    bool have = false;

    err = nvs_load_wifi(ssid, sizeof(ssid), pass, sizeof(pass), &have);
    if (err != ESP_OK || !have) {
        ESP_LOGW(TAG, "No WiFi credentials in NVS (namespace=\"%s\"). Staying offline.", WIFI_NAMESPACE);
        s_state = NET_STATE_OFFLINE;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "WiFi credentials found in NVS. Connecting to SSID=\"%s\" ...", ssid);
    s_state = NET_STATE_CONNECTING;
    return wifi_sta_start(ssid, pass);
}

net_state_t net_get_state(void)
{
    return s_state;
}

bool net_is_time_synced(void)
{
    return s_time_synced;
}
