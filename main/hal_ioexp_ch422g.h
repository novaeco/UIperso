#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "driver/i2c_master.h"

#define CH422G_I2C_PORT I2C_NUM_0
#define CH422G_I2C_ADDR 0x44

// EXIO mapping (0..7)
#define CH422G_EXIO0 0
#define CH422G_EXIO1 1
#define CH422G_EXIO2 2
#define CH422G_EXIO3 3
#define CH422G_EXIO4 4
#define CH422G_EXIO5 5
#define CH422G_EXIO6 6
#define CH422G_EXIO7 7

esp_err_t ch422g_init(void);
esp_err_t ch422g_set_pin(uint8_t pin, bool level);
esp_err_t ch422g_get_pin(uint8_t pin, bool *level);
