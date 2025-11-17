#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "driver/i2c_master.h"

#define CH422G_I2C_PORT I2C_NUM_0
#define CH422G_I2C_ADDR 0x44

esp_err_t ch422g_init(void);
esp_err_t ch422g_set_pin(uint8_t pin, bool level);
esp_err_t ch422g_get_pin(uint8_t pin, bool *level);
