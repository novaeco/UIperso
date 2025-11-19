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

static const char *TAG = "MAIN";

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void touch_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Touch button clicked");
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ESP32-S3 UI phase 3 starting");

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

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "ESP32-S3 UI Phase 3");
    lv_obj_center(label);

    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 80);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_event_cb(btn, touch_btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch me");
    lv_obj_center(btn_label);

    while (true)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
