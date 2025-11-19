#include "rgb_lcd.h"

#include <stdlib.h>
#include <string.h>

#include "esp_driver_gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "hal/lcd_types.h"

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

static lv_display_t *s_disp = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static uint8_t *s_buf1 = NULL;
static uint8_t *s_buf2 = NULL;

static void rgb_lcd_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t x1 = area->x1;
    const int32_t y1 = area->y1;
    const int32_t x2 = area->x2 + 1;
    const int32_t y2 = area->y2 + 1;

    const uint16_t *buf16 = (const uint16_t *)px_map;
    const esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel_handle, x1, y1, x2, y2, buf16);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Panel draw failed (%s)", esp_err_to_name(err));
    }

    lv_display_flush_ready(disp);
}

static void rgb_lcd_init_backlight(void)
{
    if (LCD_PIN_NUM_BACKLIGHT == GPIO_NUM_NC)
    {
        return;
    }

    const gpio_config_t cfg = {
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

    const size_t buf_size = (size_t)LCD_H_RES * LVGL_DRAW_BUF_LINES * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565);

    s_buf1 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
    s_buf2 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
    if ((s_buf1 == NULL) || (s_buf2 == NULL))
    {
        ESP_LOGE(TAG, "LVGL draw buffer allocation failed");
        abort();
    }

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
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .flags = {
            .fb_in_psram = true,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    rgb_lcd_init_backlight();

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, rgb_lcd_flush);

    ESP_LOGI(TAG, "RGB panel initialized (%dx%d)", LCD_H_RES, LCD_V_RES);
}

lv_display_t *rgb_lcd_get_disp(void)
{
    return s_disp;
}

esp_lcd_panel_handle_t rgb_lcd_get_panel(void)
{
    return s_panel_handle;
}
