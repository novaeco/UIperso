#include <stdbool.h>
#include "hal_display.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_types.h"
#include "esp_pm.h"
#if CONFIG_SPIRAM_SUPPORT
#include "esp_psram.h"
#endif
#include "esp_psram.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lvgl_port.h"
#include "hal_ioexp_ch422g.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_display";
static esp_lcd_panel_handle_t rgb_panel = NULL;
static lv_display_t *lvgl_display = NULL;

static bool rgb_panel_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    lvgl_port_flush_ready(lvgl_display);
    return false;
}

static void hal_display_power_on(void)
{
    // Enable VCOM and backlight through IO expander (active high)
    if (ch422g_init() == ESP_OK)
    {
        ch422g_set_pin(LCD_IOEX_VDD_EN, true);
        vTaskDelay(pdMS_TO_TICKS(10));
        ch422g_set_pin(LCD_IOEX_BACKLIGHT, true);
    }
    else
    {
        ESP_LOGW(TAG, "CH422G not ready, skipping LCD power enable");
    }
}

esp_err_t hal_display_init(lv_display_t **out_display)
{
    esp_err_t ret;
    hal_display_power_on();

#if CONFIG_SPIRAM_SUPPORT
    bool use_psram = esp_psram_is_initialized();
#else
    bool use_psram = false;
#endif

    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = 16,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = 30000000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 80,
            .hsync_front_porch = 80,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 20,
            .vsync_front_porch = 12,
            .flags = {
                .pclk_active_neg = false,
                .de_idle_high = false,
                .pclk_idle_high = false,
            },
        },
        .hsync_gpio_num = LCD_PIN_NUM_HSYNC,
        .vsync_gpio_num = LCD_PIN_NUM_VSYNC,
        .de_gpio_num = LCD_PIN_NUM_DE,
        .pclk_gpio_num = LCD_PIN_NUM_PCLK,
        .disp_gpio_num = LCD_PIN_NUM_DISP_EN,
        .data_gpio_nums = {
            LCD_PIN_NUM_DATA0,
            LCD_PIN_NUM_DATA1,
            LCD_PIN_NUM_DATA2,
            LCD_PIN_NUM_DATA3,
            LCD_PIN_NUM_DATA4,
            LCD_PIN_NUM_DATA5,
            LCD_PIN_NUM_DATA6,
            LCD_PIN_NUM_DATA7,
            LCD_PIN_NUM_DATA8,
            LCD_PIN_NUM_DATA9,
            LCD_PIN_NUM_DATA10,
            LCD_PIN_NUM_DATA11,
            LCD_PIN_NUM_DATA12,
            LCD_PIN_NUM_DATA13,
            LCD_PIN_NUM_DATA14,
            LCD_PIN_NUM_DATA15,
        },
        .flags = {
            .fb_in_psram = use_psram,
        },
    };

    ESP_LOGI(TAG, "Creating RGB panel");
    ret = esp_lcd_new_rgb_panel(&panel_config, &rgb_panel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "RGB panel creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = rgb_panel_on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(rgb_panel, &cbs, NULL));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(rgb_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(rgb_panel));

    size_t buffer_height = use_psram ? 80 : 40;

    lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = rgb_panel,
        .buffer_size = LCD_H_RES * buffer_height,
        .double_buffer = use_psram,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
        },
    };

    ESP_LOGI(TAG, "LVGL buffer: %s, %d lines, double buffer %s", use_psram ? "PSRAM" : "internal RAM", (int)buffer_height, use_psram ? "enabled" : "disabled");

    lvgl_display = lvgl_port_add_disp(&disp_cfg);
    if (!lvgl_display)
    {
        ESP_LOGE(TAG, "Failed to register LVGL display");
        return ESP_FAIL;
    }

    if (out_display)
    {
        *out_display = lvgl_display;
    }

    return ESP_OK;
}

void hal_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    lv_display_flush_ready(display);
    (void)area;
    (void)px_map;
}
