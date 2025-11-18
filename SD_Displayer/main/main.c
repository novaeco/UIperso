#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "rgb_lcd_port.h"
#include "gt911.h"
#include "lvgl_port.h"
#include "sd.h"
#include "wifi_manager.h"
#include "bt_manager.h"
#include "ui_main.h"
#include "storage.h"
#include "io_extension.h"

static const char *TAG = "app";

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    init_nvs();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize low level IO expander/backlight helpers early
    IO_EXTENSION_Init();

    // Initialize peripherals
    esp_lcd_touch_handle_t tp_handle = touch_gt911_init();
    esp_lcd_panel_handle_t panel_handle = waveshare_esp32_s3_rgb_lcd_init();
    wavesahre_rgb_lcd_bl_on();

    // Initialize LVGL (display + input) and register filesystem for SD card assets
    lv_display_t *display = lvgl_port_init(panel_handle, tp_handle);

    // Mount SD card and expose as LVGL FS driver
    if (sd_mmc_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "SD mount failed");
    }
    else
    {
        lvgl_port_register_fs();
    }

    // Start connectivity (Wi-Fi STA + BLE placeholder)
    wifi_manager_init();
    wifi_manager_start();
    bt_manager_init();

    // Initialize application storage (NVS backed parameters)
    storage_init();

    // Build UI and image browser
    ui_context_t ui_ctx = {0};
    ui_init(display, &ui_ctx);

    while (true)
    {
        ui_update_status(&ui_ctx);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
