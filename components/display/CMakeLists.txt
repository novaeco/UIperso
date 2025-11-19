idf_component_register(
    SRCS "rgb_lcd.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lcd driver esp_timer
)
