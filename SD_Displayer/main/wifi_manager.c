#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/apps/sntp.h"
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_mgr";
static wifi_config_runtime_t runtime_cfg = {0};
static nvs_handle_t wifi_nvs;
static bool wifi_started = false;

static void save_defaults_if_absent(void)
{
    size_t len = sizeof(runtime_cfg.ssid);
    esp_err_t err = nvs_get_str(wifi_nvs, "ssid", runtime_cfg.ssid, &len);
    if (err != ESP_OK)
    {
        strlcpy(runtime_cfg.ssid, WIFI_DEFAULT_SSID, sizeof(runtime_cfg.ssid));
        nvs_set_str(wifi_nvs, "ssid", runtime_cfg.ssid);
    }
    len = sizeof(runtime_cfg.password);
    err = nvs_get_str(wifi_nvs, "pass", runtime_cfg.password, &len);
    if (err != ESP_OK)
    {
        strlcpy(runtime_cfg.password, WIFI_DEFAULT_PASSWORD, sizeof(runtime_cfg.password));
        nvs_set_str(wifi_nvs, "pass", runtime_cfg.password);
    }
    nvs_commit(wifi_nvs);
}

static void start_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "fr.pool.ntp.org");
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    sntp_init();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        runtime_cfg.state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        runtime_cfg.state = WIFI_STATE_DISCONNECTED;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        runtime_cfg.state = WIFI_STATE_CONNECTED;
        start_sntp();
    }
}

esp_err_t wifi_manager_init(void)
{
    runtime_cfg.state = WIFI_STATE_DISCONNECTED;
    ESP_ERROR_CHECK(nvs_open("wifi_cfg", NVS_READWRITE, &wifi_nvs));
    save_defaults_if_absent();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, runtime_cfg.ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, runtime_cfg.password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (wifi_started)
    {
        return ESP_OK;
    }
    wifi_started = true;
    ESP_ERROR_CHECK(esp_wifi_start());
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    return ESP_OK;
}

wifi_config_runtime_t wifi_manager_get_state(void)
{
    return runtime_cfg;
}

void wifi_manager_request_reconnect(void)
{
    esp_wifi_disconnect();
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    esp_wifi_connect();
}
