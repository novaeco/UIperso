#include "ch422g.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_bus_shared.h"

#define CH422G_I2C_TIMEOUT_MS      i2c_bus_shared_timeout_ms()
#define CH422G_I2C_RETRIES         3
#define CH422G_I2C_RETRY_DELAY_MS  3

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
    bool io_ready;
    uint8_t outputs;
    i2c_master_dev_handle_t dev;
} s_ctx;

static esp_err_t ch422g_write_outputs(void)
{
    if (!s_ctx.io_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t payload = s_ctx.outputs;
    esp_err_t err = ESP_FAIL;

    for (int attempt = 1; attempt <= CH422G_I2C_RETRIES; ++attempt)
    {
        err = i2c_master_transmit(s_ctx.dev, &payload, sizeof(payload), CH422G_I2C_TIMEOUT_MS);

        if (err == ESP_OK)
        {
            break;
        }

        ESP_LOGW(TAG, "CH422G write attempt %d/%d failed: %s", attempt, CH422G_I2C_RETRIES, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(CH422G_I2C_RETRY_DELAY_MS));
    }

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "IO extension write failed after %d attempts (%s)", CH422G_I2C_RETRIES, esp_err_to_name(err));
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

esp_err_t ch422g_init(void)
{
    if (s_ctx.initialized)
    {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(i2c_bus_shared_init(), TAG, "Failed to init shared I2C bus");

    ESP_RETURN_ON_ERROR(i2c_bus_shared_add_device(IOEXT_I2C_ADDRESS, 100000, &s_ctx.dev), TAG, "Failed to add CH422G to shared bus");
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

