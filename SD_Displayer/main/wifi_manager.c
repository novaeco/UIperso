#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "lwip/apps/sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "nvs_flash.h"
#include "lwip/apps/sntp.h"
#include <string.h>
#include <time.h>

static const char *TAG = "wifi_mgr";
static wifi_config_runtime_t runtime_cfg = {0};
static nvs_handle_t wifi_nvs;
static bool wifi_started = false;
static bool sntp_started = false;
static esp_netif_t *wifi_netif;
static SemaphoreHandle_t wifi_mutex;

static void lock(void)
{
    if (wifi_mutex)
    {
        xSemaphoreTakeRecursive(wifi_mutex, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (wifi_mutex)
    {
        xSemaphoreGiveRecursive(wifi_mutex);
    }
}

static void start_sntp(void)
{
    if (sntp_started)
    {
        return;
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "fr.pool.ntp.org");
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    sntp_init();
    sntp_started = true;
    ESP_LOGI(TAG, "SNTP started");
}

static void mark_ntp_synced(struct timeval *tv)
{
    (void)tv;
    lock();
    runtime_cfg.ntp_synced = true;
    unlock();
}

static void save_runtime_to_nvs(void)
{
    nvs_set_str(wifi_nvs, "ssid", runtime_cfg.ssid);
    nvs_set_str(wifi_nvs, "pass", runtime_cfg.password);
    nvs_set_u8(wifi_nvs, "dhcp", runtime_cfg.use_dhcp ? 1 : 0);
    nvs_set_u8(wifi_nvs, "configured", runtime_cfg.configured ? 1 : 0);
    nvs_set_u32(wifi_nvs, "ip", runtime_cfg.ip.addr);
    nvs_set_u32(wifi_nvs, "gw", runtime_cfg.gateway.addr);
    nvs_set_u32(wifi_nvs, "mask", runtime_cfg.netmask.addr);
    nvs_commit(wifi_nvs);
}

static void load_or_seed_nvs(void)
{
    bool updated = false;
    size_t len = sizeof(runtime_cfg.ssid);
    esp_err_t err = nvs_get_str(wifi_nvs, "ssid", runtime_cfg.ssid, &len);
    if (err != ESP_OK || runtime_cfg.ssid[0] == '\0')
    {
        strlcpy(runtime_cfg.ssid, WIFI_DEFAULT_SSID, sizeof(runtime_cfg.ssid));
        updated = true;
    }


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
        updated = true;
    }

    uint8_t dhcp = 1;
    err = nvs_get_u8(wifi_nvs, "dhcp", &dhcp);
    if (err != ESP_OK)
    {
        dhcp = 1;
        updated = true;
    }
    runtime_cfg.use_dhcp = dhcp == 1;

    uint8_t configured_flag = 0;
    err = nvs_get_u8(wifi_nvs, "configured", &configured_flag);
    if (err != ESP_OK)
    {
        configured_flag = 0;
        updated = true;
    }
    runtime_cfg.configured = configured_flag != 0;

    uint32_t ip, gw, mask;
    err = nvs_get_u32(wifi_nvs, "ip", &ip);
    runtime_cfg.ip.addr = (err == ESP_OK) ? ip : 0;
    err = nvs_get_u32(wifi_nvs, "gw", &gw);
    runtime_cfg.gateway.addr = (err == ESP_OK) ? gw : 0;
    err = nvs_get_u32(wifi_nvs, "mask", &mask);
    runtime_cfg.netmask.addr = (err == ESP_OK) ? mask : 0;

    if (updated)
    {
        save_runtime_to_nvs();
    }
}

static void apply_ip_settings(void)
{
    if (!wifi_netif)
    {
        return;
    }

    if (runtime_cfg.use_dhcp)
    {
        esp_netif_dhcpc_stop(wifi_netif);
        esp_netif_dhcpc_start(wifi_netif);
    }
    else
    {
        esp_netif_dhcpc_stop(wifi_netif);
        esp_netif_ip_info_t ip_info = {
            .ip = runtime_cfg.ip,
            .netmask = runtime_cfg.netmask,
            .gw = runtime_cfg.gateway,
        };
        ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_netif, &ip_info));
    }
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
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        lock();
        runtime_cfg.state = WIFI_STATE_CONNECTING;
        unlock();
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        runtime_cfg.state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        lock();
        runtime_cfg.state = WIFI_STATE_DISCONNECTED;
        runtime_cfg.has_ip = false;
        runtime_cfg.ntp_synced = false;
        unlock();
        runtime_cfg.state = WIFI_STATE_DISCONNECTED;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        lock();
        runtime_cfg.state = WIFI_STATE_CONNECTED;
        runtime_cfg.has_ip = true;
        runtime_cfg.ip = event->ip_info.ip;
        runtime_cfg.gateway = event->ip_info.gw;
        runtime_cfg.netmask = event->ip_info.netmask;
        runtime_cfg.ntp_synced = false;
        unlock();
        runtime_cfg.state = WIFI_STATE_CONNECTED;
        start_sntp();
    }
}

esp_err_t wifi_manager_init(void)
{
    wifi_mutex = xSemaphoreCreateRecursiveMutex();
    lock();
    runtime_cfg.state = WIFI_STATE_DISCONNECTED;
    runtime_cfg.ntp_synced = false;
    runtime_cfg.has_ip = false;
    unlock();

    ESP_ERROR_CHECK(nvs_open("wifi_cfg", NVS_READWRITE, &wifi_nvs));
    load_or_seed_nvs();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_netif = esp_netif_create_default_wifi_sta();
    apply_ip_settings();

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
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    sntp_set_time_sync_notification_cb(mark_ntp_synced);
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
    lock();
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    unlock();
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    return ESP_OK;
}

wifi_config_runtime_t wifi_manager_get_state(void)
{
    wifi_config_runtime_t copy;
    lock();
    copy = runtime_cfg;
    unlock();
    return copy;
    return runtime_cfg;
}

void wifi_manager_request_reconnect(void)
{
    esp_wifi_disconnect();
    lock();
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    runtime_cfg.has_ip = false;
    runtime_cfg.ntp_synced = false;
    unlock();
    esp_wifi_connect();
}

esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password, bool use_dhcp, const esp_netif_ip_info_t *static_ip)
{
    if (!ssid || strlen(ssid) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!password)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!use_dhcp && static_ip == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    lock();
    strlcpy(runtime_cfg.ssid, ssid, sizeof(runtime_cfg.ssid));
    strlcpy(runtime_cfg.password, password, sizeof(runtime_cfg.password));
    runtime_cfg.use_dhcp = use_dhcp;
    runtime_cfg.configured = true;
    if (!use_dhcp && static_ip)
    {
        runtime_cfg.ip = static_ip->ip;
        runtime_cfg.gateway = static_ip->gw;
        runtime_cfg.netmask = static_ip->netmask;
    }
    else
    {
        runtime_cfg.ip.addr = 0;
        runtime_cfg.gateway.addr = 0;
        runtime_cfg.netmask.addr = 0;
    }
    save_runtime_to_nvs();
    unlock();

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    apply_ip_settings();
    wifi_manager_request_reconnect();
    return ESP_OK;
}

bool wifi_manager_is_configured(void)
{
    bool configured;
    lock();
    configured = runtime_cfg.configured;
    unlock();
    return configured;
}
    runtime_cfg.state = WIFI_STATE_CONNECTING;
    esp_wifi_connect();
}
