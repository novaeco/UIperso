#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "ui.h"
#include "app_ui.h"
#include "app_hw.h"

#define LVGL_TICK_PERIOD_MS 1
#define LVGL_TASK_PERIOD_MS 5
#define LVGL_BUF_LINES 40
#define LVGL_HOR_RES 1024
#define LVGL_VER_RES 600

static const char *TAG = "app_main";
static lv_display_t *disp;
static lv_indev_t *touch_indev;

static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    /* Stub : remplacer par esp_lcd RGB flush */
    (void)display;
    (void)area;
    (void)px_map;
    lv_display_flush_ready(display);
    lv_display_flush_ready(disp);
}

static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = 0;
    data->point.y = 0;
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

static void lvgl_init_display(void)
{
    size_t buf_size = LVGL_HOR_RES * LVGL_BUF_LINES * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Allocation buffers LVGL echouee");
    }

    disp = lv_display_create(LVGL_HOR_RES, LVGL_VER_RES);
    lv_display_set_flush_cb(disp, display_flush);
    lv_display_set_flush_cb(disp, display_flush, NULL);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

static void lvgl_init_touch(void)
{
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, touch_read);
    lv_indev_set_read_cb(touch_indev, touch_read, NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    hw_diag_increment_reboot_counter();

    lv_init();
    lvgl_init_display();
    lvgl_init_touch();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 2, NULL, 1);

    ui_init();
    app_ui_init_navigation();
    app_ui_show_screen(UI_SCREEN_HOME);
    app_ui_update_network_status();
    app_ui_update_storage_status();
    app_ui_update_comm_status();
    app_ui_update_diag_status();
}
