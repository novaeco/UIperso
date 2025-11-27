#include "rs485_driver.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_status.h"

#define RS485_UART_NUM UART_NUM_1
#define RS485_TXD_PIN GPIO_NUM_15  // TX -> RS485_TXD (Waveshare 7B wiring)
#define RS485_RXD_PIN GPIO_NUM_16  // RX <- RS485_RXD (Waveshare 7B wiring)
#define RS485_DE_RE_PIN GPIO_NUM_6   // Shared DE/RE, high = transmit

static const char *TAG = "RS485";
static bool s_uart_initialized = false;
static SemaphoreHandle_t s_rs485_lock = NULL;
static const uint32_t RS485_TURNAROUND_US = 80;

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

    if (s_rs485_lock == NULL)
    {
        s_rs485_lock = xSemaphoreCreateMutex();
        if (s_rs485_lock == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
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

    err = uart_driver_install(RS485_UART_NUM, 1024, 256, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = uart_set_mode(RS485_UART_NUM, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_mode half-duplex failed (%s)", esp_err_to_name(err));
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
    system_status_set_rs485_ok(true);
    ESP_LOGI(TAG, "RS485 UART initialized (UART1, 115200 8N1, half-duplex, TX buf enabled)");
    return ESP_OK;
}

esp_err_t rs485_write(const uint8_t *data, size_t len, TickType_t timeout)
{
    if (!s_uart_initialized || !data || len == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rs485_lock == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_rs485_lock, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ESP_OK;
    rs485_set_transmit(true);
    esp_rom_delay_us(50);
    int written = uart_write_bytes(RS485_UART_NUM, (const char *)data, len);
    if (written < 0 || (size_t)written != len)
    {
        ESP_LOGE(TAG, "uart_write_bytes failed");
        err = ESP_FAIL;
        goto exit;
    }

    err = uart_wait_tx_done(RS485_UART_NUM, timeout);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_wait_tx_done failed (%s)", esp_err_to_name(err));
    }
    else
    {
        system_status_add_rs485_tx(len);
    }

exit:
    esp_rom_delay_us(RS485_TURNAROUND_US);
    rs485_set_transmit(false);
    xSemaphoreGive(s_rs485_lock);
    return err;
}

esp_err_t rs485_read(uint8_t *data, size_t max_len, size_t *out_len, TickType_t timeout)
{
    if (!s_uart_initialized || !data || max_len == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rs485_lock == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_rs485_lock, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    rs485_set_transmit(false);
    int rx_bytes = uart_read_bytes(RS485_UART_NUM, data, max_len, timeout);
    if (rx_bytes < 0)
    {
        if (out_len)
        {
            *out_len = 0;
        }
        xSemaphoreGive(s_rs485_lock);
        return ESP_FAIL;
    }

    if (out_len)
    {
        *out_len = (size_t)rx_bytes;
    }

    if (rx_bytes == 0)
    {
        xSemaphoreGive(s_rs485_lock);
        return ESP_ERR_TIMEOUT;
    }
    system_status_add_rs485_rx((size_t)rx_bytes);
    xSemaphoreGive(s_rs485_lock);
    return ESP_OK;
}
