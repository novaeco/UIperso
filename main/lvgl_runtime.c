#include "lvgl_runtime.h"

#include <stdatomic.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rgb_lcd.h"
#include "ui_manager.h"

static const char *TAG = "LVGL_RUN";

static esp_timer_handle_t s_lvgl_tick_timer = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static _Atomic uint32_t s_tick_cb_count = 0;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(CONFIG_LVGL_TICK_PERIOD_US / 1000);
    atomic_fetch_add_explicit(&s_tick_cb_count, 1, memory_order_relaxed);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started (core=%d, period=%d ms)", xPortGetCoreID(), CONFIG_LVGL_HANDLER_PERIOD_MS);

    TickType_t last_heartbeat = xTaskGetTickCount();
    int64_t last_log_us = esp_timer_get_time();
    uint32_t last_tick_seen = 0;

    for (;;)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LVGL_HANDLER_PERIOD_MS));

        const int64_t now_us = esp_timer_get_time();
        if ((now_us - last_log_us) >= 1000000)
        {
            const uint32_t ticks = atomic_load_explicit(&s_tick_cb_count, memory_order_relaxed);
            const uint32_t tick_delta = ticks - last_tick_seen;
            last_tick_seen = ticks;

            const uint32_t flush_delta = rgb_lcd_flush_count_get_and_reset();
            const size_t heap_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            const size_t heap_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

            ESP_LOGI(TAG, "tick_alive=%" PRIu32 " (+%" PRIu32 "/s) flush/s=%" PRIu32 " heap_i=%u heap_psram=%u",
                     ticks, tick_delta, flush_delta, (unsigned)heap_internal, (unsigned)heap_psram);

            ui_manager_tick_1s();

            last_log_us = now_us;
            last_heartbeat = xTaskGetTickCount();
        }

        if ((xTaskGetTickCount() - last_heartbeat) >= pdMS_TO_TICKS(5000))
        {
            ESP_LOGW(TAG, "LVGL task heartbeat gap detected");
            last_heartbeat = xTaskGetTickCount();
        }
    }
}

esp_err_t lvgl_runtime_start(lv_display_t *disp)
{
    if (disp == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Init peripherals step 5: LVGL tick source");

    if (s_lvgl_tick_timer == NULL)
    {
        const esp_timer_create_args_t tick_timer_args = {
            .callback = &lvgl_tick_cb,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "lv_tick",
        };

        const esp_err_t timer_err = esp_timer_create(&tick_timer_args, &s_lvgl_tick_timer);
        if (timer_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create LVGL tick timer (%s)", esp_err_to_name(timer_err));
            return timer_err;
        }

        ESP_LOGI(TAG, "LVGL: tick create ok");
        const esp_err_t start_err = esp_timer_start_periodic(s_lvgl_tick_timer, CONFIG_LVGL_TICK_PERIOD_US);
        if (start_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start LVGL tick timer (%s)", esp_err_to_name(start_err));
            return start_err;
        }
        ESP_LOGI(TAG, "tick started (%d us)", CONFIG_LVGL_TICK_PERIOD_US);
    }
    else
    {
        ESP_LOGW(TAG, "LVGL tick timer already created; skipping");
    }

    if (s_lvgl_task_handle == NULL)
    {
        ESP_LOGI(TAG, "Creating LVGL task on core %d", 1);
        const BaseType_t lvgl_ok = xTaskCreatePinnedToCore(
            lvgl_task,
            "lvgl",
            12288,
            NULL,
            6,
            &s_lvgl_task_handle,
            1);

        if (lvgl_ok != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create LVGL task (%ld)", (long)lvgl_ok);
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGW(TAG, "LVGL task already running; skipping creation");
    }

    return ESP_OK;
}

uint32_t lvgl_tick_alive_count(void)
{
    return atomic_load_explicit(&s_tick_cb_count, memory_order_relaxed);
}

