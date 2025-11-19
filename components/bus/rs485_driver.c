#include "rs485_driver.h"

#include "esp_driver_gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#define RS485_UART_NUM UART_NUM_1
#define RS485_TXD_PIN GPIO_NUM_17  // TODO: validate pin mapping
#define RS485_RXD_PIN GPIO_NUM_18  // TODO: validate pin mapping
#define RS485_DE_RE_PIN GPIO_NUM_16  // Shared DE/RE, high = transmit

static const char *TAG = "RS485";
static bool s_uart_initialized = false;

static inline void rs485_set_transmit(bool enable)
{
    gpio_set_level(RS485_DE_RE_PIN, enable ? 1 : 0);
}

esp_err_t rs485_init(void)
{
    if (s_uart_initialized)
    {
        return ESP_OK;
    }

    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(RS485_UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(RS485_UART_NUM, RS485_TXD_PIN, RS485_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install(RS485_UART_NUM, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed (%s)", esp_err_to_name(err));
        return err;
    }

    gpio_config_t de_config = {
        .pin_bit_mask = BIT64(RS485_DE_RE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&de_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_config failed (%s)", esp_err_to_name(err));
        return err;
    }
    rs485_set_transmit(false);

    s_uart_initialized = true;
    ESP_LOGI(TAG, "RS485 UART initialized (UART1, 115200 8N1)");
    return ESP_OK;
}

esp_err_t rs485_write(const uint8_t *data, size_t len, TickType_t timeout)
{
    if (!s_uart_initialized || !data || len == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    rs485_set_transmit(true);
    int written = uart_write_bytes(RS485_UART_NUM, (const char *)data, len);
    if (written < 0 || (size_t)written != len)
    {
        rs485_set_transmit(false);
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }

    esp_err_t err = uart_wait_tx_done(RS485_UART_NUM, timeout);
    rs485_set_transmit(false);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_wait_tx_done failed (%s)", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t rs485_read(uint8_t *data, size_t max_len, size_t *out_len, TickType_t timeout)
{
    if (!s_uart_initialized || !data || max_len == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    rs485_set_transmit(false);
    int rx_bytes = uart_read_bytes(RS485_UART_NUM, data, max_len, timeout);
    if (rx_bytes < 0)
    {
        if (out_len)
        {
            *out_len = 0;
        }
        return ESP_FAIL;
    }

    if (out_len)
    {
        *out_len = (size_t)rx_bytes;
    }

    if (rx_bytes == 0)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
