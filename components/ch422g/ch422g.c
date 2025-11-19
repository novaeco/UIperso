#include "ch422g.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

#define CH422G_I2C_PORT            I2C_NUM_0
#define CH422G_I2C_SDA_GPIO        GPIO_NUM_1   // TODO: vérifier le mapping réel SDA depuis le schéma Waveshare
#define CH422G_I2C_SCL_GPIO        GPIO_NUM_2   // TODO: vérifier le mapping réel SCL depuis le schéma Waveshare
#define CH422G_I2C_SPEED_HZ        400000
#define CH422G_I2C_TIMEOUT_MS      50

#define CH422G_I2C_ADDRESS         0x40        // TODO: confirmer l'adresse exacte du CH422G sur ce design

#define CH422G_REG_CONFIG          0x01
#define CH422G_REG_OUTPUT          0x02

#define CH422G_OUTPUT_BACKLIGHT    (1U << 0)
#define CH422G_OUTPUT_LCD_POWER    (1U << 1)
#define CH422G_OUTPUT_TOUCH_RESET  (1U << 2)
#define CH422G_OUTPUT_USB_SELECT   (1U << 3)
#define CH422G_OUTPUT_SD_CS        (1U << 4)

#define CH422G_TOUCH_RESET_ACTIVE_LOW   1

static const char *TAG = "CH422G";

static struct
{
    bool initialized;
    bool bus_ready;
    uint8_t output_state;
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

static esp_err_t ch422g_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = {reg, value};
    return i2c_master_write_to_device(
        CH422G_I2C_PORT,
        CH422G_I2C_ADDRESS,
        payload,
        sizeof(payload),
        pdMS_TO_TICKS(CH422G_I2C_TIMEOUT_MS));
}

static esp_err_t ch422g_write_outputs(void)
{
    const esp_err_t err = ch422g_write_reg(CH422G_REG_OUTPUT, s_ctx.output_state);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to update CH422G outputs (%s)", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t ch422g_update_mask(uint8_t mask, bool level_high)
{
    if (level_high)
    {
        s_ctx.output_state |= mask;
    }
    else
    {
        s_ctx.output_state &= ~mask;
    }
    return ch422g_write_outputs();
}

void ch422g_init(void)
{
    if (s_ctx.initialized)
    {
        return;
    }

    ESP_ERROR_CHECK(ch422g_bus_init());

    s_ctx.output_state = 0x00;

    // Configure push-pull outputs (all push-pull by default on CH422G, but keep call for clarity).
    const esp_err_t cfg_err = ch422g_write_reg(CH422G_REG_CONFIG, 0x00);
    if (cfg_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Unable to configure CH422G mode (%s) - check board wiring", esp_err_to_name(cfg_err));
    }

    ESP_ERROR_CHECK(ch422g_write_outputs());

    s_ctx.initialized = true;
    ESP_LOGI(TAG, "CH422G initialized (addr=0x%02X)", CH422G_I2C_ADDRESS);
}

i2c_port_t ch422g_get_i2c_port(void)
{
    return CH422G_I2C_PORT;
}

esp_err_t ch422g_set_backlight(bool on)
{
    ESP_RETURN_ON_FALSE(s_ctx.bus_ready, ESP_ERR_INVALID_STATE, TAG, "CH422G not initialized");
    return ch422g_update_mask(CH422G_OUTPUT_BACKLIGHT, on);
}

esp_err_t ch422g_set_lcd_power(bool on)
{
    ESP_RETURN_ON_FALSE(s_ctx.bus_ready, ESP_ERR_INVALID_STATE, TAG, "CH422G not initialized");
    return ch422g_update_mask(CH422G_OUTPUT_LCD_POWER, on);
}

esp_err_t ch422g_set_touch_reset(bool asserted)
{
    ESP_RETURN_ON_FALSE(s_ctx.bus_ready, ESP_ERR_INVALID_STATE, TAG, "CH422G not initialized");
#if CH422G_TOUCH_RESET_ACTIVE_LOW
    const bool level_high = !asserted;
#else
    const bool level_high = asserted;
#endif
    return ch422g_update_mask(CH422G_OUTPUT_TOUCH_RESET, level_high);
}

esp_err_t ch422g_select_usb(bool usb_selected)
{
    ESP_RETURN_ON_FALSE(s_ctx.bus_ready, ESP_ERR_INVALID_STATE, TAG, "CH422G not initialized");
    // usb_selected=true routes USB, false routes CAN/alternative interface.
    return ch422g_update_mask(CH422G_OUTPUT_USB_SELECT, usb_selected);
}

esp_err_t ch422g_set_sdcard_cs(bool asserted)
{
    ESP_RETURN_ON_FALSE(s_ctx.bus_ready, ESP_ERR_INVALID_STATE, TAG, "CH422G not initialized");
    // CS assumed active low on most µSD sockets.
    const bool level_high = !asserted;
    return ch422g_update_mask(CH422G_OUTPUT_SD_CS, level_high);
}

