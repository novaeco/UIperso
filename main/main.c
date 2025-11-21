#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "driver/i2c.h"

#include "ch422g.h"
#include "gt911_touch.h"
#include "rgb_lcd.h"
#include "sdcard.h"

#include "can_driver.h"
#include "cs8501.h"
#include "rs485_driver.h"
#include "ui_manager.h"

static const char *TAG = "MAIN";

static void i2c_scan_bus(i2c_port_t port)
{
    ESP_LOGI(TAG, "I2C scan on port %d", port);
    for (uint8_t addr = 1; addr < 0x7F; ++addr)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        const esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "  - Device found at 0x%02X", addr);
        }
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ESP32-S3 UI phase 4 starting");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip model %d, %d core(s), revision %d", chip_info.model, chip_info.cores, chip_info.revision);

    esp_err_t ch_err = ch422g_init();
    if (ch_err != ESP_OK)
    {
        ESP_LOGW(TAG, "CH422G init failed (0x%x). Running without IO expander features.", ch_err);
    }

    i2c_scan_bus(ch422g_get_i2c_port());

    esp_err_t lcd_err = ch422g_set_lcd_power(true);
    if (lcd_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to enable LCD power via IO extension (%s)", esp_err_to_name(lcd_err));
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_err_t bl_err = ch422g_set_backlight(true);
    if (bl_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to enable backlight via IO extension (%s)", esp_err_to_name(bl_err));
    }

    lv_init();
    rgb_lcd_init();
    gt911_init();

    esp_err_t can_err = can_bus_init();
    if (can_err != ESP_OK)
    {
        ESP_LOGW(TAG, "CAN init failed: %s", esp_err_to_name(can_err));
    }

    esp_err_t rs485_err = rs485_init();
    if (rs485_err != ESP_OK)
    {
        ESP_LOGW(TAG, "RS485 init failed: %s", esp_err_to_name(rs485_err));
    }

    cs8501_init();
    ESP_LOGI(TAG, "Battery voltage: %.2f V, charging: %s", cs8501_get_battery_voltage(), cs8501_is_charging() ? "yes" : "no");

    esp_err_t sd_err = sdcard_init();
    if (sd_err == ESP_OK)
    {
        ESP_LOGI(TAG, "microSD mounted successfully");
        esp_err_t test_err = sdcard_test_file();
        if (test_err != ESP_OK)
        {
            ESP_LOGW(TAG, "microSD test file failed (%s)", esp_err_to_name(test_err));
        }
    }
    else
    {
        ESP_LOGW(TAG, "microSD initialization failed (%s)", esp_err_to_name(sd_err));
    }

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    ui_manager_init();

    while (true)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
