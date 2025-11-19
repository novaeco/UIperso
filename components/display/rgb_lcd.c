#include "rgb_lcd.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_rgb.h"
#include "hal/lcd_types.h"
#include "esp_log.h"

#define LCD_H_RES                  1024
#define LCD_V_RES                  600
#define LCD_PIXEL_CLOCK_HZ         (40 * 1000 * 1000) /* TODO: Validate exact clock with panel datasheet */

#define LCD_HSYNC_PULSE_WIDTH      20
#define LCD_HSYNC_BACK_PORCH       160
#define LCD_HSYNC_FRONT_PORCH      140
#define LCD_VSYNC_PULSE_WIDTH      3
#define LCD_VSYNC_BACK_PORCH       23
#define LCD_VSYNC_FRONT_PORCH      12

#define LCD_PIN_NUM_DE             GPIO_NUM_40
#define LCD_PIN_NUM_PCLK           GPIO_NUM_42
#define LCD_PIN_NUM_HSYNC          GPIO_NUM_39
#define LCD_PIN_NUM_VSYNC          GPIO_NUM_41
#define LCD_PIN_NUM_BACKLIGHT      GPIO_NUM_38

#define LCD_DATA_WIDTH             16
#define LVGL_DRAW_BUF_LINES        100

static const gpio_num_t s_data_gpio[LCD_DATA_WIDTH] = {
    GPIO_NUM_4,  GPIO_NUM_5,  GPIO_NUM_6,  GPIO_NUM_7,
    GPIO_NUM_8,  GPIO_NUM_9,  GPIO_NUM_10, GPIO_NUM_11,
    GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15,
    GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19,
};

static const char *TAG = "RGB_LCD";

static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_buf1 = NULL;
static lv_color_t *s_buf2 = NULL;
static lv_disp_drv_t s_disp_drv;
static lv_disp_t *s_disp = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;

static void rgb_lcd_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)disp_drv->user_data;
    const int32_t x1 = area->x1;
    const int32_t y1 = area->y1;
    const int32_t x2 = area->x2 + 1;
    const int32_t y2 = area->y2 + 1;
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, color_p);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Panel draw failed (%s)", esp_err_to_name(err));
    }
    lv_disp_flush_ready(disp_drv);
}

static void rgb_lcd_init_backlight(void)
{
    if (LCD_PIN_NUM_BACKLIGHT == GPIO_NUM_NC)
    {
        return;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(LCD_PIN_NUM_BACKLIGHT, 1);
}

void rgb_lcd_init(void)
{
    if (s_panel_handle)
    {
        return;
    }

    const size_t draw_buf_pixels = LV_HOR_RES_MAX * LVGL_DRAW_BUF_LINES;
    const size_t draw_buf_size_bytes = draw_buf_pixels * sizeof(lv_color_t);

    s_buf1 = (lv_color_t *)heap_caps_malloc(draw_buf_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_buf2 = (lv_color_t *)heap_caps_malloc(draw_buf_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if ((s_buf1 == NULL) || (s_buf2 == NULL))
    {
        ESP_LOGE(TAG, "LVGL draw buffer allocation failed");
        abort();
    }

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, draw_buf_pixels);

    esp_lcd_rgb_timing_t timing = {
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .h_res = LCD_H_RES,
        .v_res = LCD_V_RES,
        .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
        .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
        .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
        .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
        .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
        .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
        .flags = {
            .pclk_active_neg = true,
            .hsync_idle_low = true,
            .vsync_idle_low = true,
            .de_idle_high = false,
            .pclk_idle_high = false,
        },
    };

    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = LCD_DATA_WIDTH,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .disp_gpio_num = GPIO_NUM_NC,
        .pclk_gpio_num = LCD_PIN_NUM_PCLK,
        .vsync_gpio_num = LCD_PIN_NUM_VSYNC,
        .hsync_gpio_num = LCD_PIN_NUM_HSYNC,
        .de_gpio_num = LCD_PIN_NUM_DE,
        .data_gpio_nums = {
            s_data_gpio[0],  s_data_gpio[1],  s_data_gpio[2],  s_data_gpio[3],
            s_data_gpio[4],  s_data_gpio[5],  s_data_gpio[6],  s_data_gpio[7],
            s_data_gpio[8],  s_data_gpio[9],  s_data_gpio[10], s_data_gpio[11],
            s_data_gpio[12], s_data_gpio[13], s_data_gpio[14], s_data_gpio[15],
        },
        .timings = timing,
        .bounce_buffer_size_px = LV_HOR_RES_MAX * 10,
        .flags = {
            .fb_in_psram = true,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    rgb_lcd_init_backlight();

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LCD_H_RES;
    s_disp_drv.ver_res = LCD_V_RES;
    s_disp_drv.flush_cb = rgb_lcd_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.user_data = s_panel_handle;
    s_disp_drv.sw_rotate = 0;
    s_disp_drv.full_refresh = 0;

    s_disp = lv_disp_drv_register(&s_disp_drv);
    ESP_LOGI(TAG, "RGB panel initialized (%dx%d)", LCD_H_RES, LCD_V_RES);
}

lv_disp_t *rgb_lcd_get_disp(void)
{
    return s_disp;
}

esp_lcd_panel_handle_t rgb_lcd_get_panel(void)
{
    return s_panel_handle;
}
