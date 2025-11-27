#include "can_driver.h"

#include <inttypes.h>
#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "system_status.h"

#define CAN_TX_GPIO GPIO_NUM_20
#define CAN_RX_GPIO GPIO_NUM_19

static const char *TAG = "CAN_DRV";
static bool s_can_started = false;

static twai_timing_config_t select_timing_config(void)
{
#if defined(CONFIG_CAN_BITRATE_125K)
    return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
#elif defined(CONFIG_CAN_BITRATE_250K)
    return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
#elif defined(CONFIG_CAN_BITRATE_1M)
    return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
#else
    return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
#endif
}

static const char *select_bitrate_label(void)
{
#if defined(CONFIG_CAN_BITRATE_125K)
    return "125 kbit/s";
#elif defined(CONFIG_CAN_BITRATE_250K)
    return "250 kbit/s";
#elif defined(CONFIG_CAN_BITRATE_1M)
    return "1 Mbit/s";
#else
    return "500 kbit/s";
#endif
}

esp_err_t can_bus_init(void)
{
    if (s_can_started)
    {
        ESP_LOGI(TAG, "CAN bus already started; skipping init");
        return ESP_OK;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO,
#if defined(CONFIG_CAN_LISTEN_ONLY) && CONFIG_CAN_LISTEN_ONLY
                                                                  TWAI_MODE_LISTEN_ONLY);
    ESP_LOGI(TAG, "TWAI listen-only mode enabled (CONFIG_CAN_LISTEN_ONLY)");
#else
                                                                  TWAI_MODE_NORMAL);
#endif
    g_config.tx_queue_len = 5;
    g_config.rx_queue_len = 5;
    g_config.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_ERR_PASS | TWAI_ALERT_RX_DATA | TWAI_ALERT_TX_SUCCESS;

    const twai_timing_config_t t_config = select_timing_config();
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "twai_driver_install failed: %s (CAN désactivé)", esp_err_to_name(err));
        system_status_set_can_ok(false);
        return err;
    }

    if (err == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "TWAI driver already installed; skipping install");
    }

    err = twai_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "twai_start failed: %s (CAN désactivé)", esp_err_to_name(err));
        system_status_set_can_ok(false);
        return err;
    }

    if (err == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "TWAI driver already started; skipping start");
    }

    s_can_started = true;
    system_status_set_can_ok(true);
    ESP_LOGI(TAG, "TWAI bus initialized (%s)", select_bitrate_label());
    return ESP_OK;
}

esp_err_t can_bus_send_test_frame(void)
{
    if (!s_can_started)
    {
        ESP_LOGW(TAG, "TWAI driver not started");
        return ESP_ERR_INVALID_STATE;
    }

    twai_message_t tx_msg = {
        .identifier = 0x123,
        .extd = 0,
        .rtr = 0,
        .data_length_code = 8,
    };
    for (int i = 0; i < tx_msg.data_length_code; ++i)
    {
        tx_msg.data[i] = i;
    }

    esp_err_t err = twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send TWAI test frame (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "TWAI test frame queued");
    return ESP_OK;
}

esp_err_t can_bus_receive_frame(twai_message_t *message, TickType_t ticks_to_wait)
{
    if (!s_can_started)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = twai_receive(message, ticks_to_wait);
    if (err == ESP_OK)
    {
        ESP_LOGD(TAG, "TWAI frame received: ID=0x%03" PRIx32 ", DLC=%d", message->identifier, message->data_length_code);
    }
    return err;
}
