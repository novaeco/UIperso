#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "app_hw.h"

static const char *TAG = "app_main";
static lv_display_t *s_disp = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;

#define LCD_H_RES 1024
#define LCD_V_RES 600
#define LVGL_BUF_PIX (LCD_H_RES * LCD_V_RES / 5)

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

static bool lcd_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (s_panel) {
        esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    }
    lv_display_flush_ready(disp);
    return true;
}

static void init_display_driver(void) {
    // Configuration RGB générique 1024x600 60Hz, buffers en PSRAM
    size_t buf_size = LVGL_BUF_PIX * sizeof(lv_color_t);
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Allocation buffers LVGL échouée");
        abort();
    }

    esp_lcd_rgb_panel_config_t rgb_config = {
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .disp_gpio_num = GPIO_NUM_NC,
        .pclk_hz = 14000000,
        .timings = {
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 8,
            .hsync_back_porch = 8,
            .hsync_front_porch = 40,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 4,
            .vsync_front_porch = 20,
            .pclk_active_neg = false,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(s_disp, lcd_flush_callback);
    lv_display_set_buffers(s_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

static bool touch_read_stub(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    LV_UNUSED(drv);
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = 0;
    data->point.y = 0;
    return false;
}

static void init_touch_driver(void) {
    // Placeholder GT911 -> LVGL indev
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_stub;
    lv_indev_drv_register(&indev_drv);
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
