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
#if CONFIG_ENABLE_SDCARD
#include "sdcard.h"
#endif
#include "sdkconfig.h"

#include "can_driver.h"
#include "cs8501.h"
#include "rs485_driver.h"
#include "ui_manager.h"
#include "logs_panel.h"
#include "ui_smoke.h"

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

static const char *TAG_INIT = "APP_INIT";
static const char *TAG = "MAIN";
static const char *TAG_LVGL = "LVGL";

static void app_init_task(void *arg);
static void log_build_info(void);
static void log_option_state(void);
static void lvgl_task(void *arg);
static void lvgl_runtime_start(lv_display_t *disp);
static void exio4_toggle_selftest(void);
static void log_storage_state(bool storage_available);
static void publish_hw_status(bool i2c_ok, bool ch422g_ok, bool gt911_ok, bool touch_available);

#define INIT_YIELD()            \
    do {                        \
        vTaskDelay(pdMS_TO_TICKS(1)); \
    } while (0)
static inline int64_t stage_begin(const char *stage)
{
    int64_t ts = esp_timer_get_time();
    ESP_LOGI(TAG_INIT, "%s start", stage);
    return ts;
}

static inline void stage_end(const char *stage, int64_t start_ts)
{
    int64_t elapsed_ms = (esp_timer_get_time() - start_ts) / 1000;
    ESP_LOGI(TAG_INIT, "%s done in %lld ms", stage, (long long)elapsed_ms);
}

static void log_build_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG_INIT, "ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG_INIT, "Chip model %d, %d core(s), revision %d", chip_info.model, chip_info.cores, chip_info.revision);
}

static void log_option_state(void)
{
    ESP_LOGI(TAG_INIT, "Options: display=%d, touch=%d, sdcard=%d, i2c_scan=%d, ui_smoke=%d",
             CONFIG_ENABLE_DISPLAY,
             CONFIG_ENABLE_TOUCH,
             CONFIG_ENABLE_SDCARD,
             CONFIG_I2C_SCAN_AT_BOOT,
             CONFIG_UI_SMOKE_MODE);
}

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
#if !CONFIG_I2C_SCAN_AT_BOOT
    ESP_LOGI(TAG, "I2C scan skipped (CONFIG_I2C_SCAN_AT_BOOT=0)");
    return;
#endif

    if (bus == NULL)
    {
        ESP_LOGW(TAG, "I2C scan skipped: bus not initialized");
        return;
    }

    ESP_LOGI(TAG, "I2C scan on shared bus");
    logs_panel_add_log("Scan I2C sur bus partagé");
    const esp_err_t lock_err = i2c_bus_lock(pdMS_TO_TICKS(i2c_bus_shared_timeout_ms()));
    if (lock_err != ESP_OK)
    {
        ESP_LOGW(TAG, "I2C scan lock failed: %s", esp_err_to_name(lock_err));
        return;
    }

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

    i2c_bus_unlock();

    if (devices == 0)
    {
        logs_panel_add_log("I2C: aucun périphérique détecté");
    }
}

static esp_timer_handle_t s_lvgl_tick_timer = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void log_storage_state(bool storage_available)
{
    ESP_LOGI(TAG, "storage_available=%d", storage_available ? 1 : 0);
    logs_panel_add_log("Stockage externe: %s", storage_available ? "disponible" : "absent");
}

static void publish_hw_status(bool i2c_ok, bool ch422g_ok, bool gt911_ok, bool touch_available)
{
    ui_manager_set_bus_status(i2c_ok, ch422g_ok, gt911_ok);
    ui_manager_set_touch_available(touch_available);
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
    BaseType_t ok = xTaskCreatePinnedToCore(
        app_init_task,
        "app_init",
        12288,
        NULL,
        5,
        NULL,
        0);

    if (ok != pdPASS) {
        ESP_LOGE(TAG_INIT, "Failed to create app_init_task");
    }

    vTaskDelete(NULL);
}

static void app_init_task(void *arg)
{
    ESP_LOGI(TAG_INIT, "app_init_task starting on core=%d", xPortGetCoreID());

    bool degraded_mode = false;
    bool storage_available = false;
    bool i2c_ok = false;
    bool ch422g_ok = false;
    bool gt911_ok = false;
    bool touch_available = false;
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ESP32-S3 UI phase 4 starting");
    log_build_info();
    log_option_state();
    log_reset_diagnostics();
    INIT_YIELD();

    esp_err_t i2c_err = i2c_bus_shared_init();
    if (i2c_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Shared I2C bus init failed: %s", esp_err_to_name(i2c_err));
        logs_panel_add_log("I2C partagé: échec (%s)", esp_err_to_name(i2c_err));
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        i2c_ok = true;
        logs_panel_add_log("I2C partagé: prêt");
    }
    publish_hw_status(i2c_ok, ch422g_ok, gt911_ok, touch_available);

    esp_err_t ch_err = ch422g_init();
    if (ch_err != ESP_OK)
    {
        ESP_LOGW(TAG, "CH422G init failed (0x%x). Running without IO expander features.", ch_err);
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        ch422g_ok = ch422g_is_available();
    }
    publish_hw_status(i2c_ok, ch422g_ok, gt911_ok, touch_available);
    INIT_YIELD();

    if (i2c_ok)
    {
        i2c_scan_bus(i2c_bus_shared_handle());
    }
    INIT_YIELD();

    if (ch422g_is_available())
    {
        exio4_toggle_selftest();

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
        INIT_YIELD();

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

    // Bring up LVGL and the RGB panel before optional peripherals so the SMOKE UI is always visible.
    lv_init();
    ESP_LOGI(TAG, "After lv_init(), before RGB LCD creation");
    INIT_YIELD();

    int64_t t_rgb = stage_begin("rgb_lcd_init");
    rgb_lcd_init();
    stage_end("rgb_lcd_init", t_rgb);
    ESP_LOGI(TAG, "RGB LCD init done, retrieving lv_display_t handle");
    INIT_YIELD();

    lv_display_t *disp = rgb_lcd_get_disp();
    if (disp == NULL)
    {
        ESP_LOGE(TAG, "RGB display not available; skipping touch init");
        logs_panel_add_log("Afficheur LVGL indisponible : tactile désactivé");
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        lv_display_set_default(disp);
        ESP_LOGI(TAG, "MAIN: default LVGL display set to %p", (void *)disp);
        lvgl_runtime_start(disp);
        ui_smoke_init(disp);
    }

#if CONFIG_ENABLE_SDCARD
    ESP_LOGI(TAG, "Init peripherals step 1: sdcard_init() after LCD ready");
    // Silence noisy IDF-level errors from the VFS FAT SDMMC helper when the slot is empty.
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_NONE);
    int64_t t_sd = stage_begin("sdcard_init");
    esp_err_t sd_err = sdcard_init();
    stage_end("sdcard_init", t_sd);
    if (sd_err == ESP_OK)
    {
        ESP_LOGI(TAG, "microSD mounted successfully");
        storage_available = true;
        esp_err_t test_err = sdcard_test_file();
        if (test_err != ESP_OK)
        {
            ESP_LOGW(TAG, "microSD test file failed (%s)", esp_err_to_name(test_err));
        }
    }
    else if (sd_err == ESP_ERR_TIMEOUT || sd_err == ESP_ERR_NOT_FOUND)
    {
        ESP_LOGW(TAG, "microSD unavailable (%s); continuing without external storage", esp_err_to_name(sd_err));
    }
    else
    {
        ESP_LOGE(TAG, "microSD initialization failed (%s); continuing without external storage", esp_err_to_name(sd_err));
    }
    INIT_YIELD();
#else
    ESP_LOGI(TAG, "Init peripherals step 1: microSD disabled (CONFIG_ENABLE_SDCARD=0); skipping sdcard_init()");
    logs_panel_add_log("microSD désactivée (menuconfig)");
#endif

    log_storage_state(storage_available);

    // Yield after synchronous storage probing so IDLE tasks can run before touch and the rest.
    vTaskDelay(pdMS_TO_TICKS(1));

#if CONFIG_ENABLE_TOUCH
    ESP_LOGI(TAG, "Before GT911 init (display %s)", disp ? "ready" : "missing");
    logs_panel_add_log("Init GT911 en cours");
    esp_err_t touch_err = gt911_init(disp);
    if (touch_err != ESP_OK || !gt911_is_initialized())
    {
        ESP_LOGW(TAG, "GT911 init failed (%s); input device not registered", esp_err_to_name(touch_err));
        logs_panel_add_log("GT911: init échouée (%s), tactile indisponible", esp_err_to_name(touch_err));
        touch_available = gt911_touch_available();
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        gt911_ok = true;
        touch_available = gt911_touch_available();
        ESP_LOGI(TAG, "GT911 successfully attached to LVGL (touch enabled)");
    }
#else
    ESP_LOGW(TAG, "GT911 disabled at compile-time (CONFIG_ENABLE_TOUCH=0); skipping touch attachment");
    logs_panel_add_log("GT911 désactivé (CONFIG_ENABLE_TOUCH=0)");
    touch_available = false;
    degraded_mode = true;
    ui_manager_set_degraded(true);
#endif
    publish_hw_status(i2c_ok, ch422g_ok, gt911_ok, touch_available);
    ESP_LOGI(TAG_INIT, "I2C ok=%d, CH422G ok=%d, GT911 ok=%d, touch_available=%d",
             i2c_ok,
             ch422g_ok,
             gt911_ok,
             touch_available);
    logs_panel_add_log("État bus: I2C=%d CH422G=%d GT911=%d", i2c_ok, ch422g_ok, gt911_ok);
    ESP_LOGI(TAG, "After GT911 init, proceeding with peripherals");

    // Avoid monopolizing CPU0 around synchronous peripheral inits.
    INIT_YIELD();

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
    INIT_YIELD();

    ESP_LOGI(TAG, "Init peripherals step 3: rs485_init()");
    esp_err_t rs485_err = rs485_init();
    if (rs485_err != ESP_OK)
    {
        log_non_fatal_error("RS485 init", rs485_err);
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    INIT_YIELD();

    ESP_LOGI(TAG, "Init peripherals step 4: cs8501_init()");
    cs8501_init();
    ESP_LOGI(TAG, "Battery voltage: %.2f V, charging: %s", cs8501_get_battery_voltage(), cs8501_is_charging() ? "yes" : "no");
    INIT_YIELD();

    ESP_LOGI(TAG, "Init peripherals step 6: ui_manager_init()");
    ESP_LOGI(TAG, "UI: entrypoint called: ui_manager_init");
    int64_t t_ui = stage_begin("ui_manager_init");
    esp_err_t ui_err = ui_manager_init();
    stage_end("ui_manager_init", t_ui);

    if (ui_err != ESP_OK)
    {
        log_non_fatal_error("UI manager init", ui_err);
        degraded_mode = true;
        ui_manager_set_degraded(true);
    }
    else
    {
        ui_manager_set_degraded(degraded_mode);
        ESP_LOGI(TAG, "MAIN: ui init done");
        ESP_LOGI(TAG, "MAIN: active screen=%p", (void *)lv_scr_act());
        lv_obj_invalidate(lv_screen_active());
    }

    ESP_LOGI(TAG_INIT, "Phase 1 done: display=%s, touch=%s, sdcard=%s",
             lv_disp_get_default() ? "ready" : "missing",
             gt911_is_initialized() ? "ready" : "disabled",
             storage_available ? "mounted" : "absent");

    log_heap_metrics("post-init");

    vTaskDelete(NULL);
}

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG_LVGL, "LVGL task started");
    ESP_LOGI(TAG_LVGL, "task pinned core=%d", xPortGetCoreID());

    TickType_t last_heartbeat = xTaskGetTickCount();
    uint32_t heartbeat_counter = 0;

    for (;;)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));

        const TickType_t now = xTaskGetTickCount();
        if ((now - last_heartbeat) >= pdMS_TO_TICKS(1000))
        {
            heartbeat_counter++;
            last_heartbeat = now;
            ESP_LOGI(TAG_LVGL, "LVGL heartbeat %u", (unsigned int)heartbeat_counter);
        }
    }
}

static void lvgl_runtime_start(lv_display_t *disp)
{
    (void)disp;
    ESP_LOGI(TAG, "Init peripherals step 5: LVGL tick source");

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
    };

    if (s_lvgl_tick_timer == NULL)
    {
        esp_err_t timer_err = esp_timer_create(&tick_timer_args, &s_lvgl_tick_timer);
        if (timer_err != ESP_OK)
        {
            ESP_LOGE(TAG_LVGL, "Failed to create LVGL tick timer (%s)", esp_err_to_name(timer_err));
        }
        else
        {
            ESP_LOGI(TAG_LVGL, "tick create ok");
            timer_err = esp_timer_start_periodic(s_lvgl_tick_timer, 1000);
            if (timer_err != ESP_OK)
            {
                ESP_LOGE(TAG_LVGL, "Failed to start LVGL tick timer (%s)", esp_err_to_name(timer_err));
            }
            else
            {
                ESP_LOGI(TAG_LVGL, "tick started (1ms)");
            }
        }
    }
    else
    {
        ESP_LOGW(TAG_LVGL, "LVGL tick timer already created; skipping");
    }

    if (s_lvgl_task_handle == NULL)
    {
        ESP_LOGI(TAG_INIT, "app_init_task continuing: creating LVGL task on core %d", 1);
        BaseType_t lvgl_ok = xTaskCreatePinnedToCore(
            lvgl_task,
            "lvgl",
            12288,
            NULL,
            6,
            &s_lvgl_task_handle,
            1);

        if (lvgl_ok != pdPASS)
        {
            ESP_LOGE(TAG_INIT, "Failed to create LVGL task; stopping app_init_task");
        }
    }
    else
    {
        ESP_LOGW(TAG_LVGL, "LVGL task already running; skipping creation");
    }
}

static void exio4_toggle_selftest(void)
{
    ESP_LOGI(TAG, "EXIO4 self-test: toggling CS every 200 ms for 2 s (active low)");
    const TickType_t delay_ticks = pdMS_TO_TICKS(200);
    for (int i = 0; i < 10; ++i)
    {
        const bool assert_cs = (i & 0x01) == 0;
        esp_err_t err = ch422g_set_sdcard_cs(assert_cs);
        ESP_LOGI(TAG, "EXIO4 %s (expected %s)", assert_cs ? "LOW" : "HIGH", assert_cs ? "assert" : "release");
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "EXIO4 toggle %d failed (%s)", i, esp_err_to_name(err));
        }
        vTaskDelay(delay_ticks);
    }

    // Leave CS released at the end of the self-test
    (void)ch422g_set_sdcard_cs(false);
}
