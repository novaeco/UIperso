#include "can_driver.h"

#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_log.h"

#define CAN_TX_GPIO GPIO_NUM_43  // TODO: confirm against Waveshare schematics
#define CAN_RX_GPIO GPIO_NUM_44  // TODO: confirm against Waveshare schematics

static const char *TAG = "CAN_DRV";
static bool s_can_started = false;

esp_err_t can_bus_init(void)
{
    if (s_can_started)
    {
        return ESP_OK;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_LOOPBACK);
    g_config.tx_queue_len = 5;
    g_config.rx_queue_len = 5;
    g_config.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_ERR_PASS | TWAI_ALERT_RX_DATA | TWAI_ALERT_TX_SUCCESS;

    const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install TWAI driver (%s)", esp_err_to_name(err));
        return err;
    }

    err = twai_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start TWAI driver (%s)", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    s_can_started = true;
    ESP_LOGI(TAG, "TWAI bus initialized (loopback mode, 500 kbit/s)");
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
