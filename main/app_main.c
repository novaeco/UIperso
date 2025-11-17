#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lvgl.h"

#include "ui.h"
#include "app_hw.h"

static const char *TAG = "app_main";

static void lv_tick_cb(void *arg) {
    LV_UNUSED(arg);
    lv_tick_inc(10);
}

static void init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void init_display_driver(void) {
    // TODO: implémenter driver RGB 1024x600 + buffers DMA/PSRAM
    // Exemple : enregistrer lv_display_t* avec lv_display_create();
}

static void init_touch_driver(void) {
    // TODO: implémenter driver GT911 -> lv_indev_drv_t
}

void app_main(void) {
    init_nvs();
    ESP_LOGI(TAG, "Booting UI terrariophile");

    lv_init();

    const esp_timer_create_args_t tick_args = {
        .callback = &lv_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 10 * 1000)); // 10ms

    init_display_driver();
    init_touch_driver();

    ui_init();

    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
