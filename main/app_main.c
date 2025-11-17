#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "hal_sdcard.h"
#include "hal_can.h"
#include "hal_rs485.h"
#include "hal_ioexp_ch422g.h"
#include "ui/ui_init.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "app_main";
static SemaphoreHandle_t lvgl_mutex;

static void lvgl_tick(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "Initializing hardware");

    lvgl_mutex = xSemaphoreCreateMutex();

    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_priority = 4;
    lvgl_cfg.task_stack = 8192;
    lvgl_cfg.timer_period_ms = 5;
    lvgl_cfg.task_affinity = 1;
    lvgl_cfg.task_max_sleep_ms = 100;
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    lv_display_t *display = NULL;
    lv_indev_t *indev = NULL;
    ESP_ERROR_CHECK(hal_display_init(&display));
    ESP_ERROR_CHECK(hal_touch_init(display, &indev));

    ESP_ERROR_CHECK(hal_sdcard_init());
    ESP_ERROR_CHECK(hal_can_init());
    ESP_ERROR_CHECK(hal_rs485_init());
    ESP_ERROR_CHECK(ch422g_init());

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    ESP_LOGI(TAG, "Starting UI");
    ui_init(display, indev);

    while (true)
    {
        lvgl_port_lock(0);
        lv_timer_handler();
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
