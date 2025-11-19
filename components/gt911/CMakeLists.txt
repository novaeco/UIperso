idf_component_register(
    SRCS "gt911_touch.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_driver_gpio esp_driver_i2c ch422g driver
)
