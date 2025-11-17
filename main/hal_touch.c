#include "hal_touch.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "hal_display.h"
#include "esp_lcd_panel_io.h"

static const char *TAG = "hal_touch";
static esp_lcd_touch_handle_t tp = NULL;
static lv_indev_t *lvgl_indev = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;

i2c_master_bus_handle_t hal_touch_get_i2c_bus(void)
{
    return i2c_bus;
}

esp_err_t hal_touch_init(lv_display_t *display, lv_indev_t **out_indev)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz = 400000;
    esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST_GPIO,
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
    };

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TOUCH_I2C_PORT,
        .scl_io_num = TOUCH_I2C_SCL,
        .sda_io_num = TOUCH_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &touch_config, &tp));

    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = display,
        .handle = tp,
    };
    lvgl_indev = lvgl_port_add_touch(&touch_cfg);

    if (!lvgl_indev)
    {
        ESP_LOGE(TAG, "Failed to register LVGL touch");
        return ESP_FAIL;
    }

    if (out_indev)
    {
        *out_indev = lvgl_indev;
    }

    return ESP_OK;
}

void hal_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint8_t touch_cnt = 0;
    esp_lcd_touch_read_data(tp);
    bool touched = esp_lcd_touch_get_coordinates(tp, touch_x, touch_y, NULL, &touch_cnt, 1);
    if (touched && touch_cnt > 0)
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch_x[0];
        data->point.y = touch_y[0];
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
    (void)indev;
}
