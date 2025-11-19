idf_component_register(
    SRCS "rgb_lcd.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lcd esp_driver_gpio esp_timer
)
