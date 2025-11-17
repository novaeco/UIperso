#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "driver/i2c_master.h"

#define TOUCH_I2C_PORT I2C_NUM_0
#define TOUCH_I2C_SCL 9
#define TOUCH_I2C_SDA 10
#define TOUCH_INT_GPIO 11
#define TOUCH_RST_GPIO 12

esp_err_t hal_touch_init(lv_display_t *display, lv_indev_t **out_indev);
void hal_touch_read(lv_indev_t *indev, lv_indev_data_t *data);
i2c_master_bus_handle_t hal_touch_get_i2c_bus(void);
