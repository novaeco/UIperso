#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "ch422g.h"
#include "gt911_touch.h"
#include "rgb_lcd.h"
#include "sdcard.h"

#include "can_driver.h"
#include "cs8501.h"
#include "rs485_driver.h"
#include "ui_manager.h"

static const char *TAG = "MAIN";

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

    ch422g_init();
    ESP_ERROR_CHECK(ch422g_set_lcd_power(true));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(ch422g_set_backlight(true));

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
