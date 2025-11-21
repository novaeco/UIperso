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
#include "logs_panel.h"

static const char *TAG = "MAIN";

// Power-on sequencing derived from ST7262 / Waveshare timing (VDD -> DISP/backlight)
#define LCD_POWER_STABILIZE_DELAY_MS   20

static const char *reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt WDT";
    case ESP_RST_TASK_WDT:
        return "task WDT";
    case ESP_RST_WDT:
        return "other WDT";
    case ESP_RST_DEEPSLEEP:
        return "deep sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "unknown";
    }
}

static void log_reset_diagnostics(void)
{
    const esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %s (%d)", reset_reason_to_str(reason), reason);
    logs_panel_add_log("Reset: %s (%d)", reset_reason_to_str(reason), reason);

    if (reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT)
    {
        ESP_LOGW(TAG, "Last reset was triggered by a watchdog (reason=%d). Review long-running tasks and IRQ load.", reason);
        logs_panel_add_log("WDT reset détecté : examiner les tâches longues et IRQ");
    }
}

static void i2c_scan_bus(i2c_port_t port)
{
    ESP_LOGI(TAG, "I2C scan on port %d", port);
    logs_panel_add_log("Scan I2C sur port %d", port);
    int devices = 0;
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
            logs_panel_add_log("I2C: périphérique détecté @0x%02X", addr);
            devices++;
        }
    }

    if (devices == 0)
    {
        logs_panel_add_log("I2C: aucun périphérique détecté");
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
    log_reset_diagnostics();

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip model %d, %d core(s), revision %d", chip_info.model, chip_info.cores, chip_info.revision);

    esp_err_t ch_err = ch422g_init();
    if (ch_err != ESP_OK)
    {
        ESP_LOGW(TAG, "CH422G init failed (0x%x). Running without IO expander features.", ch_err);
    }

    i2c_scan_bus(ch422g_get_i2c_port());

    if (ch422g_is_available())
    {
        esp_err_t lcd_err = ch422g_set_lcd_power(true);
        if (lcd_err != ESP_OK)
        {
            ESP_LOGW(TAG, "EXIO6 (LCD_VDD_EN) enable failed (%s); continuing degraded", esp_err_to_name(lcd_err));
            logs_panel_add_log("LCD_VDD_EN: échec (%s)", esp_err_to_name(lcd_err));
        }
        else
        {
            ESP_LOGI(TAG, "EXIO6 (LCD_VDD_EN) asserted; waiting %d ms before DISP/backlight", LCD_POWER_STABILIZE_DELAY_MS);
            logs_panel_add_log("LCD_VDD_EN activé, délai %d ms", LCD_POWER_STABILIZE_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(LCD_POWER_STABILIZE_DELAY_MS));
        }

        esp_err_t bl_err = ch422g_set_backlight(true);
        if (bl_err != ESP_OK)
        {
            ESP_LOGW(TAG, "EXIO2 (DISP/backlight) enable failed (%s); UI will start without panel", esp_err_to_name(bl_err));
            logs_panel_add_log("Backlight: échec (%s)", esp_err_to_name(bl_err));
        }
        else
        {
            ESP_LOGI(TAG, "EXIO2 (DISP/backlight) asserted after power stabilization");
            logs_panel_add_log("Backlight activé après stabilisation");
        }
    }
    else
    {
        ESP_LOGW(TAG, "IO extension unavailable; skipping LCD_VDD_EN / DISP sequencing (UI will start degraded)");
        logs_panel_add_log("Extenseur IO indisponible : séquence LCD sautée");
    }

    lv_init();
    rgb_lcd_init();
    logs_panel_add_log("Init GT911 en cours");
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
