#include "hal_ioexp_ch422g.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "hal_touch.h"

static const char *TAG = "ch422g";
static bool initialized = false;
static uint8_t output_state = 0x00;
static i2c_master_dev_handle_t ch422g_dev = NULL;

static esp_err_t ch422g_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(ch422g_dev, payload, sizeof(payload), pdMS_TO_TICKS(100));
}

esp_err_t ch422g_init(void)
{
    if (initialized)
    {
        return ESP_OK;
    }
    i2c_master_bus_handle_t bus = hal_touch_get_i2c_bus();
    if (!bus)
    {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &ch422g_dev), TAG, "Failed to add CH422G device");

    initialized = true;
    return ch422g_write_reg(0x01, output_state);
}

esp_err_t ch422g_set_pin(uint8_t pin, bool level)
{
    if (pin > 7)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized)
    {
        ESP_RETURN_ON_ERROR(ch422g_init(), TAG, "I2C init failed");
    }
    if (level)
    {
        output_state |= (1 << pin);
    }
    else
    {
        output_state &= ~(1 << pin);
    }
    return ch422g_write_reg(0x01, output_state);
}

esp_err_t ch422g_get_pin(uint8_t pin, bool *level)
{
    if (pin > 7 || !level)
    {
        return ESP_ERR_INVALID_ARG;
    }
    *level = (output_state >> pin) & 0x1;
    return ESP_OK;
}
