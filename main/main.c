#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "driver/i2c_master.h"

#include "ch422g.h"
#include "gt911_touch.h"
#include "i2c_bus_shared.h"
#include "rgb_lcd.h"
#include "sdcard.h"

#include "can_driver.h"
#include "cs8501.h"
#include "rs485_driver.h"
#include "ui_manager.h"
#include "logs_panel.h"

/*
 * Touch reboot loop post-mortem:
 * - Root cause observed: ui_manager_init() could run while the default LVGL
 *   display handle was missing or invalid, triggering an LVGL assert that
 *   bubbled up as esp_restart_noos (rst:0xc / panic reason 4) immediately
 *   after "After GT911 init, proceeding with peripherals".
 * - Fixes applied: guard UI creation behind a display check, replace hard
 *   failure paths with degraded-mode logging, and instrument each peripheral
 *   init step so the last executed block is visible in logs.
 * - Expected behavior: if touch or display are unavailable, the system stays
 *   alive in degraded mode (UI visible when display is present; no reset on
 *   non-critical errors) instead of rebooting.
 */

#ifndef GT911_ENABLE
#define GT911_ENABLE 1
#endif

static const char *TAG = "MAIN";

// Reset loop root-cause (panic reason=4) was LVGL tick double-counting when CONFIG_LV_TICK_CUSTOM=1
// coexisted with the esp_timer-driven lv_tick_inc(1) callback. Keep the custom tick forcefully
// disabled here so the esp_timer path remains the single tick source.
// Use only the ESP-IDF Kconfig symbol to avoid referencing undefined LVGL-side macros.
#if defined(CONFIG_LV_TICK_CUSTOM) && (CONFIG_LV_TICK_CUSTOM != 0)
#error "CONFIG_LV_TICK_CUSTOM must be disabled (0) when using esp_timer-driven lv_tick_inc()"
#endif

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

static void i2c_scan_bus(i2c_master_bus_handle_t bus)
{
    if (bus == NULL)
    {
        ESP_LOGW(TAG, "I2C scan skipped: bus not initialized");
        return;
    }

    ESP_LOGI(TAG, "I2C scan on shared bus");
    logs_panel_add_log("Scan I2C sur bus partagé");
    int devices = 0;
    for (uint8_t addr = 1; addr < 0x7F; ++addr)
    {
        const esp_err_t err = i2c_master_probe(bus, addr, i2c_bus_shared_timeout_ms());

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "  - Device found at 0x%02X", addr);
            logs_panel_add_log("I2C: périphérique détecté @0x%02X", addr);
            devices++;
        }

        if ((addr & 0x07) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
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

static inline void log_non_fatal_error(const char *what, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed: %s (non-fatal, degraded mode)", what, esp_err_to_name(err));
    logs_panel_add_log("%s: échec (%s)", what, esp_err_to_name(err));
}

static void log_heap_metrics(const char *stage)
{
    const size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t dram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG,
             "Heap (%s): DRAM free=%u (min=%u) bytes, PSRAM free=%u (min=%u) bytes",
             stage,
             (unsigned int)dram_free,
             (unsigned int)dram_min,
             (unsigned int)psram_free,
             (unsigned int)psram_min);

    logs_panel_add_log("Heap %s: DRAM %u/%u, PSRAM %u/%u",
                       stage,
                       (unsigned int)dram_free,
                       (unsigned int)dram_min,
                       (unsigned int)psram_free,
                       (unsigned int)psram_min);
}

void app_main(void)
{
    bool degraded_mode = false;
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
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }

    i2c_scan_bus(i2c_bus_shared_handle());

    if (ch422g_is_available())
    {
        esp_err_t lcd_err = ch422g_set_lcd_power(true);
        if (lcd_err != ESP_OK)
        {
            ESP_LOGW(TAG, "EXIO6 (LCD_VDD_EN) enable failed (%s); continuing degraded", esp_err_to_name(lcd_err));
            logs_panel_add_log("LCD_VDD_EN: échec (%s)", esp_err_to_name(lcd_err));
            degraded_mode = true;
            ui_manager_set_degraded(true);
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
            degraded_mode = true;
            ui_manager_set_degraded(true);
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
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }

    ESP_LOGI(TAG, "After CH422G/LCD sequencing, before LVGL core init");

    ESP_LOGI(TAG, "Init peripherals step 1: sdcard_init() before RGB panel start");
    // Silence noisy IDF-level errors from the VFS FAT SDMMC helper when the slot is empty.
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_NONE);
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
    else if (sd_err != ESP_ERR_TIMEOUT && sd_err != ESP_ERR_NOT_FOUND)
    {
        ESP_LOGE(TAG, "microSD initialization failed (%s)", esp_err_to_name(sd_err));
    }

    // Yield after synchronous storage probing so IDLE tasks can run before display/touch bring-up.
    vTaskDelay(pdMS_TO_TICKS(1));

    // LVGL + display must be ready before GT911 so the indev can bind to the default display
    lv_init();
    ESP_LOGI(TAG, "After lv_init(), before RGB LCD creation");

    rgb_lcd_init();
    ESP_LOGI(TAG, "RGB LCD init done, retrieving lv_display_t handle");

    lv_display_t *disp = rgb_lcd_get_disp();
    if (disp == NULL)
    {
        ESP_LOGE(TAG, "RGB display not available; skipping touch init");
        logs_panel_add_log("Afficheur LVGL indisponible : tactile désactivé");
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }

#if GT911_ENABLE
    ESP_LOGI(TAG, "Before GT911 init (display %s)", disp ? "ready" : "missing");
    logs_panel_add_log("Init GT911 en cours");
    esp_err_t touch_err = gt911_init(disp);
    if (touch_err != ESP_OK || !gt911_is_initialized())
    {
        ESP_LOGW(TAG, "GT911 init failed (%s); input device not registered", esp_err_to_name(touch_err));
        logs_panel_add_log("GT911: init échouée (%s), tactile indisponible", esp_err_to_name(touch_err));
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        ESP_LOGI(TAG, "GT911 successfully attached to LVGL (touch enabled)");
    }
#else
    ESP_LOGW(TAG, "GT911 disabled at compile-time (GT911_ENABLE=0); skipping touch attachment");
    logs_panel_add_log("GT911 désactivé (GT911_ENABLE=0)");
    degraded_mode = true;
    ui_manager_set_degraded(true);
#endif
    ESP_LOGI(TAG, "After GT911 init, proceeding with peripherals");

    // Avoid monopolizing CPU0 around synchronous peripheral inits.
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_LOGI(TAG, "Init peripherals step 2: can_bus_init()");

    esp_err_t can_err = can_bus_init();
    if (can_err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "CAN init failed: %s (CAN désactivé, on continue sans CAN)",
                 esp_err_to_name(can_err));
        logs_panel_add_log("CAN: init échouée (%s), CAN désactivé", esp_err_to_name(can_err));
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        ESP_LOGI(TAG, "CAN init OK, bus actif");
    }

    ESP_LOGI(TAG, "Init peripherals step 3: rs485_init()");
    esp_err_t rs485_err = rs485_init();
    if (rs485_err != ESP_OK)
    {
        log_non_fatal_error("RS485 init", rs485_err);
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }

    ESP_LOGI(TAG, "Init peripherals step 4: cs8501_init()");
    cs8501_init();
    ESP_LOGI(TAG, "Battery voltage: %.2f V, charging: %s", cs8501_get_battery_voltage(), cs8501_is_charging() ? "yes" : "no");

    ESP_LOGI(TAG, "Init peripherals step 5: LVGL tick source");
#if LV_TICK_CUSTOM
    /*
     * Prevent the historical double-tick panic that used to occur right after
     * "GT911 ready" when LV_TICK_CUSTOM (LVGL side) was enabled while the
     * esp_timer-driven lv_tick_inc(1) callback was also running. If a custom
     * tick is configured, skip the esp_timer source instead of rebooting on an
     * LVGL assert.
     */
    ESP_LOGW(TAG, "LV_TICK_CUSTOM is enabled; skipping esp_timer LVGL tick source to avoid double counting");
#else
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    esp_err_t timer_err = esp_timer_create(&tick_timer_args, &tick_timer);
    if (timer_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create LVGL tick timer (%s)", esp_err_to_name(timer_err));
    }
    else
    {
        timer_err = esp_timer_start_periodic(tick_timer, 1000);
        if (timer_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start LVGL tick timer (%s)", esp_err_to_name(timer_err));
        }
        else
        {
            ESP_LOGI(TAG, "LVGL tick: esp_timer 1 ms (LV_TICK_CUSTOM=0)");
        }
    }
#endif

    ESP_LOGI(TAG, "Init peripherals step 6: ui_manager_init()");
    esp_err_t ui_err = ui_manager_init();
    if (ui_err != ESP_OK)
    {
        log_non_fatal_error("UI manager init", ui_err);
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        ui_manager_set_degraded(degraded_mode);
    }

    log_heap_metrics("post-init");

    while (true)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
