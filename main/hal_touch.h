#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "driver/i2c_master.h"

#define TOUCH_I2C_PORT I2C_NUM_0
#define TOUCH_I2C_SCL 9
#define TOUCH_I2C_SDA 8
#define TOUCH_INT_GPIO 4

// IO expander controlled reset pin
#define TOUCH_RST_IOEX 1

esp_err_t hal_touch_bus_init(void);
esp_err_t hal_touch_init(lv_display_t *display, lv_indev_t **out_indev);
void hal_touch_read(lv_indev_t *indev, lv_indev_data_t *data);
i2c_master_bus_handle_t hal_touch_get_i2c_bus(void);
