#include "hal_ioexp_ch422g.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "hal_touch.h"

static const char *TAG = "ch422g";
static bool initialized = false;
static uint8_t output_state = 0x00;

static esp_err_t ch422g_write_reg(uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CH422G_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(CH422G_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ch422g_init(void)
{
    if (initialized)
    {
        return ESP_OK;
    }
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    esp_err_t ret = i2c_param_config(CH422G_I2C_PORT, &cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }
    ret = i2c_driver_install(CH422G_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }
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
