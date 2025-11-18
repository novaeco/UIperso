#pragma once

#include "esp_err.h"
#include "lvgl.h"

#define LCD_H_RES 1024
#define LCD_V_RES 600

// RGB data pins and control pins based on Waveshare ESP32-S3-Touch-LCD-7B
// Data mapping follows RGB565: B3-B7, G2-G7, R3-R7
#define LCD_PIN_NUM_HSYNC 46
#define LCD_PIN_NUM_VSYNC 3
#define LCD_PIN_NUM_DE -1
#define LCD_PIN_NUM_PCLK 7
#define LCD_PIN_NUM_DISP_EN -1
#define LCD_PIN_NUM_BACKLIGHT -1

#define LCD_PIN_NUM_DATA0 14  // B3
#define LCD_PIN_NUM_DATA1 38  // B4
#define LCD_PIN_NUM_DATA2 18  // B5
#define LCD_PIN_NUM_DATA3 17  // B6
#define LCD_PIN_NUM_DATA4 10  // B7
#define LCD_PIN_NUM_DATA5 39  // G2
#define LCD_PIN_NUM_DATA6 0   // G3
#define LCD_PIN_NUM_DATA7 45  // G4
#define LCD_PIN_NUM_DATA8 48  // G5
#define LCD_PIN_NUM_DATA9 47  // G6
#define LCD_PIN_NUM_DATA10 21 // G7
#define LCD_PIN_NUM_DATA11 1  // R3
#define LCD_PIN_NUM_DATA12 2  // R4
#define LCD_PIN_NUM_DATA13 42 // R5
#define LCD_PIN_NUM_DATA14 41 // R6
#define LCD_PIN_NUM_DATA15 40 // R7

// IO expander pins for power rails
#define LCD_IOEX_VDD_EN 6
#define LCD_IOEX_BACKLIGHT 2

esp_err_t hal_display_init(lv_display_t **out_display);
void hal_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);

