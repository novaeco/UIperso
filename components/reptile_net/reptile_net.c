#include "reptile_net.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

static const char *TAG = "REPTILE_NET";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     5

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    reptile_wifi_state_t *state = (reptile_wifi_state_t *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < WIFI_MAX_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Reconnexion Wi-Fi (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        state->connected = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        state->ip = event->ip_info.ip;
        state->connected = true;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connecté, IP=" IPSTR, IP2STR(&state->ip));
    }
}

esp_err_t reptile_net_prefs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        esp_err_t erase_err = nvs_flash_erase();
        if (erase_err != ESP_OK)
        {
            return erase_err;
        }
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t reptile_net_prefs_save_wifi(const reptile_wifi_credentials_t *creds)
{
    if (!creds)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(handle, "ssid", creds->ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_str(handle, "pass", creds->password);
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t reptile_net_prefs_load_wifi(reptile_wifi_credentials_t *creds)
{
    if (!creds)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    size_t len_ssid = sizeof(creds->ssid);
    size_t len_pass = sizeof(creds->password);
    err = nvs_get_str(handle, "ssid", creds->ssid, &len_ssid);
    if (err == ESP_OK)
    {
        err = nvs_get_str(handle, "pass", creds->password, &len_pass);
    }

    nvs_close(handle);
    return err;
}

esp_err_t reptile_net_wifi_start(const reptile_wifi_credentials_t *creds, reptile_wifi_state_t *state_out)
{
    if (!creds || !state_out)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        return ret;
    }

    s_wifi_event_group = xEventGroupCreate();

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, state_out, NULL);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, state_out, NULL);
    if (ret != ESP_OK)
    {
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password));

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        return ret;
    }

    ESP_LOGI(TAG, "Connexion Wi-Fi démarrée vers SSID='%s'", creds->ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Wi-Fi connecté, IP=" IPSTR, IP2STR(&state_out->ip));
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGW(TAG, "Échec de connexion Wi-Fi après %d tentatives", s_retry_num);
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGW(TAG, "Délai de connexion Wi-Fi dépassé");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t reptile_net_ntp_start(const reptile_ntp_config_t *config)
{
    if (!config || !config->server)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->tz)
    {
        setenv("TZ", config->tz, 1);
        tzset();
    }

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, config->server);
    sntp_init();
    ESP_LOGI(TAG, "NTP initialisé via %s", config->server);
    return ESP_OK;
}

