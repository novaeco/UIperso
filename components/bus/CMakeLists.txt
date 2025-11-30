idf_component_register(
    SRCS "i2c_bus_shared.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_i2c esp_timer log driver
)
