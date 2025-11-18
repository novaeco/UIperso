#include "hal_can.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "hal_ioexp_ch422g.h"

static const char *TAG = "hal_can";
static bool can_started = false;

esp_err_t hal_can_init(void)
{
    // Select CAN transceiver mode via IO expander (EXIO5 high selects CAN)
    if (ch422g_init() == ESP_OK)
    {
        ch422g_set_pin(CH422G_EXIO5, true);
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g_config.clkout_divider = 0;
    g_config.tx_queue_len = 5;
    g_config.rx_queue_len = 5;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "CAN install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = twai_start();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "CAN start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    can_started = true;
    return ESP_OK;
}

esp_err_t hal_can_send_test_frame(void)
{
    if (!can_started)
    {
        return ESP_ERR_INVALID_STATE;
    }
    twai_message_t message = {
        .identifier = 0x321,
        .data_length_code = 8,
        .data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE},
        .flags = TWAI_MSG_FLAG_NONE,
    };
    return twai_transmit(&message, pdMS_TO_TICKS(100));
}

esp_err_t hal_can_receive(twai_message_t *out_message, TickType_t ticks_to_wait)
{
    if (!can_started)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return twai_receive(out_message, ticks_to_wait);
}

bool hal_can_is_started(void)
{
    return can_started;
}
