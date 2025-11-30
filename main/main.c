#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "sdkconfig.h"
#include "project_config_compat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

#include "ui_manager.h"
#include "logs_panel.h"
#include "system_status.h"
#include "ui_smoke.h"
#include "lvgl_runtime.h"
#include "reptile_core.h"
#include "reptile_net.h"

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

static void app_init_task(void *arg);
static void log_build_info(void);
static void log_option_state(void);
static void update_ui_mode_from_status(void);
static void exio4_toggle_selftest(void);
static void log_storage_state(bool storage_available);
static void publish_hw_status(bool i2c_ok, bool ch422g_ok, bool gt911_ok, bool touch_available);
static void log_active_screen_state(const char *stage);
static void persistence_task(void *arg);
static void request_save_data(const char *audit);
static void request_audit(const char *audit);
static void request_wifi_connect(const reptile_wifi_credentials_t *creds);

typedef struct
{
    lv_display_t *disp;
    const system_status_t *status_ref;
    const ui_manager_ctx_t *ctx;
    esp_err_t result;
} ui_init_ctx_t;

typedef enum
{
    APP_REQ_SAVE_DATA,
    APP_REQ_APPEND_AUDIT,
    APP_REQ_WIFI_CONNECT,
} app_request_type_t;

typedef struct
{
    app_request_type_t type;
    char audit_msg[96];
    reptile_wifi_credentials_t wifi_creds;
} app_request_t;

static reptiles_data_t s_reptiles = {0};
static reptile_wifi_state_t s_wifi_state = {0};
static reptile_wifi_credentials_t s_wifi_creds = {0};
static const char *s_storage_mount = "/sdcard";
static QueueHandle_t s_app_queue = NULL;

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

static void log_active_screen_state(const char *stage)
{
    lv_display_t *disp = lv_disp_get_default();
    lv_obj_t *screen = lv_screen_active();
    const uint32_t children = screen ? lv_obj_get_child_cnt(screen) : 0;

    ESP_LOGI(TAG, "%s: default_disp=%p screen=%p children=%u", stage, (void *)disp, (void *)screen, (unsigned)children);
}

static void enqueue_request(const app_request_t *req)
{
    if (req == NULL || s_app_queue == NULL)
    {
        return;
    }

    if (xQueueSend(s_app_queue, req, pdMS_TO_TICKS(10)) != pdPASS)
    {
        ESP_LOGW(TAG, "App queue full; dropping request type=%d", (int)req->type);
    }
}

static void request_save_data(const char *audit)
{
    app_request_t req = {
        .type = APP_REQ_SAVE_DATA,
    };
    if (audit)
    {
        strlcpy(req.audit_msg, audit, sizeof(req.audit_msg));
    }
    enqueue_request(&req);
}

static void request_audit(const char *audit)
{
    app_request_t req = {
        .type = APP_REQ_APPEND_AUDIT,
    };
    if (audit)
    {
        strlcpy(req.audit_msg, audit, sizeof(req.audit_msg));
    }
    enqueue_request(&req);
}

static void request_wifi_connect(const reptile_wifi_credentials_t *creds)
{
    if (creds == NULL)
    {
        return;
    }
    app_request_t req = {
        .type = APP_REQ_WIFI_CONNECT,
    };
    memcpy(&req.wifi_creds, creds, sizeof(req.wifi_creds));
    enqueue_request(&req);
}

static void lvgl_create_smoke_cb(void *arg)
{
    lv_display_t *disp = (lv_display_t *)arg;
    ui_smoke_boot_screen();
    ui_smoke_init(disp);
    log_active_screen_state("SMOKE_INIT");
}

static void lvgl_load_fallback_cb(void *arg)
{
    (void)arg;
    ui_smoke_fallback();
    log_active_screen_state("SMOKE_FALLBACK");
}

static void lvgl_ui_manager_init_cb(void *arg)
{
    ui_init_ctx_t *ctx = (ui_init_ctx_t *)arg;
    ctx->result = ui_manager_init(ctx->disp, ctx->status_ref, ctx->ctx);
    log_active_screen_state("UI_MANAGER_INIT");
}

static void lvgl_diag_screen_cb(void *arg)
{
    (void)arg;
    ui_smoke_diag_screen();
    log_active_screen_state("SMOKE_DIAG");
}

static void lvgl_post_ui_cb(void *arg)
{
    (void)arg;
    log_active_screen_state("UI_POST_INIT");
    lv_obj_invalidate(lv_screen_active());
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


static void log_storage_state(bool storage_available)
{
    ESP_LOGI(TAG, "storage_available=%d", storage_available ? 1 : 0);
    logs_panel_add_log("Stockage externe: %s", storage_available ? "disponible" : "absent");
    system_status_set_sd_mounted(storage_available);
    update_ui_mode_from_status();
}

static void update_ui_mode_from_status(void)
{
    system_status_t status = {0};
    system_status_get(&status);
    if (!status.touch_available)
    {
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
    }
    else if (!status.sd_mounted)
    {
        ui_manager_set_mode(UI_MODE_DEGRADED_SD);
    }
    else
    {
        ui_manager_set_mode(UI_MODE_NORMAL);
    }
}

static void publish_hw_status(bool i2c_ok, bool ch422g_ok, bool gt911_ok, bool touch_available)
{
    (void)i2c_ok;
    (void)ch422g_ok;
    (void)gt911_ok;
    system_status_set_touch_available(touch_available);
    update_ui_mode_from_status();
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

static void persistence_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Persistence task running (queue depth=%d)", (int)uxQueueSpacesAvailable(s_app_queue));

    for (;;)
    {
        app_request_t req = {0};
        if (xQueueReceive(s_app_queue, &req, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        switch (req.type)
        {
        case APP_REQ_SAVE_DATA:
            ESP_LOGI(TAG, "Saving reptile dataset (%d entries)", s_reptiles.reptile_count);
            (void)reptile_core_save(&s_reptiles, s_storage_mount);
            if (req.audit_msg[0] != '\0')
            {
                (void)reptile_core_audit_append(s_storage_mount, req.audit_msg);
            }
            break;
        case APP_REQ_APPEND_AUDIT:
            if (req.audit_msg[0] != '\0')
            {
                (void)reptile_core_audit_append(s_storage_mount, req.audit_msg);
            }
            break;
        case APP_REQ_WIFI_CONNECT:
        {
            ESP_LOGI(TAG, "Wi-Fi connect request for SSID='%s'", req.wifi_creds.ssid);
            (void)reptile_net_prefs_save_wifi(&req.wifi_creds);
            esp_err_t wifi_err = reptile_net_wifi_start(&req.wifi_creds, &s_wifi_state);
            system_status_set_wifi(wifi_err == ESP_OK, wifi_err == ESP_OK ? (const char *)req.wifi_creds.ssid : "");
            if (wifi_err == ESP_OK)
            {
                reptile_ntp_config_t ntp_cfg = {
                    .server = "pool.ntp.org",
                    .tz = "CET-1CEST,M3.5.0,M10.5.0/3",
                };
                (void)reptile_net_ntp_start(&ntp_cfg);
                (void)reptile_core_audit_append(s_storage_mount, "Wi-Fi connecté, NTP lancé");
            }
            else
            {
                ESP_LOGW(TAG, "Wi-Fi connect failed: %s", esp_err_to_name(wifi_err));
                (void)reptile_core_audit_append(s_storage_mount, "Wi-Fi: échec de connexion");
            }
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown request type=%d", req.type);
            break;
        }
    }
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
    system_status_init();
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
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
            ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
            ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
    }
    else
    {
        lv_display_set_default(disp);
        ESP_LOGI(TAG, "MAIN: default LVGL display set to %p", (void *)disp);

        const int64_t t_lvgl_runtime = stage_begin("lvgl_runtime_start");
        esp_err_t lvgl_start_err = lvgl_runtime_start(disp);
        stage_end("lvgl_runtime_start", t_lvgl_runtime);

        if (lvgl_start_err != ESP_OK)
        {
            ESP_LOGE(TAG, "LVGL runtime start failed (%s)", esp_err_to_name(lvgl_start_err));
        }
        else
        {
            ESP_LOGI(TAG, "LVGL tick alive=%" PRIu32 " immediately after start", lvgl_tick_alive_count());
        }

        if (lvgl_runtime_wait_started(500))
        {
            ESP_LOGI(TAG, "LVGL handler task confirmed running; creating smoke UI");
        }
        else
        {
            ESP_LOGW(TAG, "LVGL handler task not confirmed after 500 ms; continuing");
        }

#if CONFIG_DIAG_LVGL_SOLID_SCREEN
        (void)lvgl_runtime_dispatch(lvgl_load_fallback_cb, NULL, true);
        esp_err_t smoke_err = lvgl_runtime_dispatch(lvgl_create_smoke_cb, disp, true);
        if (smoke_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to build smoke UI on LVGL task (%s)", esp_err_to_name(smoke_err));
        }
        (void)lvgl_runtime_dispatch(lvgl_diag_screen_cb, NULL, true);
#else
        esp_err_t smoke_err = lvgl_runtime_dispatch(lvgl_create_smoke_cb, disp, true);
        if (smoke_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to build smoke UI on LVGL task (%s)", esp_err_to_name(smoke_err));
            (void)lvgl_runtime_dispatch(lvgl_load_fallback_cb, NULL, true);
        }
#endif
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
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
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
    ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
#endif
    publish_hw_status(i2c_ok, ch422g_ok, gt911_ok, touch_available);
    ESP_LOGI(TAG_INIT, "I2C ok=%d, CH422G ok=%d, GT911 ok=%d, touch_available=%d",
             i2c_ok,
             ch422g_ok,
             gt911_ok,
             touch_available);
    logs_panel_add_log("État bus: I2C=%d CH422G=%d GT911=%d", i2c_ok, ch422g_ok, gt911_ok);
    ESP_LOGI(TAG, "After GT911 init, proceeding with peripherals");

    system_status_set_wifi(false, "");

    if (s_app_queue == NULL)
    {
        s_app_queue = xQueueCreate(8, sizeof(app_request_t));
        if (s_app_queue)
        {
            (void)xTaskCreatePinnedToCore(persistence_task, "persist", 4096, NULL, 4, NULL, 1);
        }
    }

    ESP_LOGI(TAG, "Init data model + prefs");
    reptile_net_prefs_init();

    esp_err_t core_err = reptile_core_init(&s_reptiles, s_storage_mount);
    if (core_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Reptile core init failed (%s), dataset empty", esp_err_to_name(core_err));
    }

    if (reptile_net_prefs_load_wifi(&s_wifi_creds) == ESP_OK)
    {
        ESP_LOGI(TAG, "Wi-Fi creds loaded for SSID '%s'", s_wifi_creds.ssid);
        request_wifi_connect(&s_wifi_creds);
    }

    ESP_LOGI(TAG, "Init peripherals step 6: ui_manager_init()");
    ESP_LOGI(TAG, "UI: entrypoint called: ui_manager_init");
    int64_t t_ui = stage_begin("ui_manager_init");
    ui_manager_ctx_t manager_ctx = {
        .storage_mount = s_storage_mount,
        .reptiles = &s_reptiles,
        .wifi_state = &s_wifi_state,
        .wifi_creds = &s_wifi_creds,
        .save_data_cb = request_save_data,
        .audit_cb = request_audit,
        .wifi_connect_cb = request_wifi_connect,
    };

    ui_init_ctx_t ui_ctx = {
        .disp = disp,
        .status_ref = system_status_get_ref(),
        .ctx = &manager_ctx,
        .result = ESP_FAIL,
    };

    esp_err_t ui_dispatch_err = lvgl_runtime_dispatch(lvgl_ui_manager_init_cb, &ui_ctx, true);
    esp_err_t ui_err = (ui_dispatch_err == ESP_OK) ? ui_ctx.result : ui_dispatch_err;
    stage_end("ui_manager_init", t_ui);

    if (ui_err != ESP_OK)
    {
        log_non_fatal_error("UI manager init", ui_err);
        degraded_mode = true;
        ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
        (void)lvgl_runtime_dispatch(lvgl_load_fallback_cb, NULL, true);
    }
    else
    {
        if (degraded_mode)
        {
            ui_manager_set_mode(UI_MODE_DEGRADED_TOUCH);
        }
        else
        {
            update_ui_mode_from_status();
        }
        ESP_LOGI(TAG, "MAIN: ui init done");
        (void)lvgl_runtime_dispatch(lvgl_post_ui_cb, NULL, true);
    }

    ESP_LOGI(TAG_INIT, "Phase 1 done: display=%s, touch=%s, sdcard=%s",
             lv_disp_get_default() ? "ready" : "missing",
             gt911_is_initialized() ? "ready" : "disabled",
             storage_available ? "mounted" : "absent");

    log_heap_metrics("post-init");

    vTaskDelete(NULL);
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
