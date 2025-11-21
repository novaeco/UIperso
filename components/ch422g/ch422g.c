#include "ch422g.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

#define CH422G_I2C_PORT            I2C_NUM_0
#define CH422G_I2C_SDA_GPIO        GPIO_NUM_8   // Waveshare ESP32-S3-Touch-LCD-7B shared I2C SDA
#define CH422G_I2C_SCL_GPIO        GPIO_NUM_9   // Waveshare ESP32-S3-Touch-LCD-7B shared I2C SCL
#define CH422G_I2C_SPEED_HZ        400000
#define CH422G_I2C_TIMEOUT_MS      50

static const char *TAG = "CH422G";

static struct
{
    bool initialized;
    bool bus_ready;
} s_ctx;

static esp_err_t ch422g_bus_init(void)
{
    if (s_ctx.bus_ready)
    {
        return ESP_OK;
    }

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CH422G_I2C_SDA_GPIO,
        .scl_io_num = CH422G_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CH422G_I2C_SPEED_HZ,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(CH422G_I2C_PORT, &cfg), TAG, "i2c_param_config failed");

    esp_err_t err = i2c_driver_install(CH422G_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE))
    {
        ESP_LOGE(TAG, "i2c_driver_install failed (%s)", esp_err_to_name(err));
        return err;
    }

    s_ctx.bus_ready = true;
    return ESP_OK;
}

esp_err_t ch422g_init(void)
{
    if (s_ctx.initialized)
    {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ch422g_bus_init(), TAG, "Failed to init I2C bus");
    s_ctx.initialized = true;
    ESP_LOGW(TAG, "IO extension stub active: CH422G hardware not present. I2C bus initialized for shared peripherals.");
    return ESP_OK;
}

i2c_port_t ch422g_get_i2c_port(void)
{
    return CH422G_I2C_PORT;
}

esp_err_t ch422g_set_backlight(bool on)
{
    ESP_LOGI(TAG, "Backlight request (%s) ignored: IO extension not implemented", on ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t ch422g_set_lcd_power(bool on)
{
    ESP_LOGI(TAG, "LCD power request (%s) ignored: IO extension not implemented", on ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t ch422g_set_touch_reset(bool asserted)
{
    ESP_LOGI(TAG, "Touch reset request (%s) ignored: IO extension not implemented", asserted ? "assert" : "release");
    return ESP_OK;
}

esp_err_t ch422g_select_usb(bool usb_selected)
{
    ESP_LOGI(TAG, "USB/CAN select request (%s) ignored: IO extension not implemented", usb_selected ? "USB" : "CAN");
    return ESP_OK;
}

esp_err_t ch422g_set_sdcard_cs(bool asserted)
{
    ESP_LOGI(TAG, "SD CS request (%s) ignored: IO extension not implemented", asserted ? "assert" : "deassert");
    return ESP_OK;
}

