#include "ch422g.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

#define CH422G_I2C_PORT            I2C_NUM_0
#define CH422G_I2C_SDA_GPIO        GPIO_NUM_8   // Waveshare ESP32-S3-Touch-LCD-7B shared I2C SDA
#define CH422G_I2C_SCL_GPIO        GPIO_NUM_9   // Waveshare ESP32-S3-Touch-LCD-7B shared I2C SCL
#define CH422G_I2C_SPEED_HZ        400000
#define CH422G_I2C_TIMEOUT_TICKS   0            // Non-blocking: do not wait if the bus is busy

#define IOEXT_I2C_ADDRESS          0x24         // CH32V003 (Waveshare IO extension)

// Bitfield mapping for the external IO expander (EXIO1..EXIO8 -> bit0..bit7)
#define EXIO1_BIT                  (1U << 0)    // TP_RST (active low)
#define EXIO2_BIT                  (1U << 1)    // DISP / backlight enable
#define EXIO4_BIT                  (1U << 3)    // SD_CS (active low)
#define EXIO5_BIT                  (1U << 4)    // USB_SEL / CAN_SEL (high = USB)
#define EXIO6_BIT                  (1U << 5)    // LCD_VDD_EN

static const char *TAG = "CH422G";

static struct
{
    bool initialized;
    bool bus_ready;
    bool io_ready;
    uint8_t outputs;
} s_ctx;

static esp_err_t ch422g_write_outputs(void)
{
    if (!s_ctx.io_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t payload = s_ctx.outputs;
    const esp_err_t err = i2c_master_write_to_device(
        CH422G_I2C_PORT,
        IOEXT_I2C_ADDRESS,
        &payload,
        sizeof(payload),
        CH422G_I2C_TIMEOUT_TICKS);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "IO extension write failed (%s)", esp_err_to_name(err));
    }

    return err;
}

static esp_err_t ch422g_set_output_bit(uint8_t bit_mask, bool high)
{
    if (!s_ctx.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ctx.io_ready)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (high)
    {
        s_ctx.outputs |= bit_mask;
    }
    else
    {
        s_ctx.outputs &= (uint8_t)~bit_mask;
    }

    return ch422g_write_outputs();
}

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
    s_ctx.io_ready = true;

    // Initialize outputs to a safe default (all released/disabled)
    s_ctx.outputs = 0;
    esp_err_t err = ch422g_write_outputs();
    if (err != ESP_OK)
    {
        s_ctx.io_ready = false;
        return err;
    }

    s_ctx.initialized = true;
    ESP_LOGI(TAG, "IO extension ready on addr 0x%02X", IOEXT_I2C_ADDRESS);
    return ESP_OK;
}

i2c_port_t ch422g_get_i2c_port(void)
{
    return CH422G_I2C_PORT;
}

bool ch422g_is_available(void)
{
    return s_ctx.io_ready;
}

esp_err_t ch422g_set_backlight(bool on)
{
    return ch422g_set_output_bit(EXIO2_BIT, on);
}

esp_err_t ch422g_set_lcd_power(bool on)
{
    return ch422g_set_output_bit(EXIO6_BIT, on);
}

esp_err_t ch422g_set_touch_reset(bool asserted)
{
    // EXIO1 is active low: asserted => drive low
    return ch422g_set_output_bit(EXIO1_BIT, !asserted);
}

esp_err_t ch422g_select_usb(bool usb_selected)
{
    return ch422g_set_output_bit(EXIO5_BIT, usb_selected);
}

esp_err_t ch422g_set_sdcard_cs(bool asserted)
{
    // EXIO4 is active low: asserted => drive low
    return ch422g_set_output_bit(EXIO4_BIT, !asserted);
}

bool ch422g_sdcard_cs_available(void)
{
    return s_ctx.io_ready;
}

