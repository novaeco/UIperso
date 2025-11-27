idf_component_register(
    SRCS "can_driver.c" "rs485_driver.c" "i2c_bus_shared.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_uart esp_driver_twai esp_driver_i2c esp_timer log driver system_status
)
