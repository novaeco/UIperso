#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared I2C0 bus used by onboard peripherals.
 *
 * Idempotent: returns ESP_OK if the bus was already configured.
 */
esp_err_t i2c_bus_shared_init(void);

/**
 * @brief Get the handle to the shared master bus.
 */
i2c_master_bus_handle_t i2c_bus_shared_handle(void);

/**
 * @brief Default transaction timeout in milliseconds for the shared bus.
 */
int i2c_bus_shared_timeout_ms(void);

/**
 * @brief Attach an I2C device to the shared bus with the given address/speed.
 */
esp_err_t i2c_bus_shared_add_device(uint16_t address,
                                    uint32_t scl_speed_hz,
                                    i2c_master_dev_handle_t *ret_handle);

#ifdef __cplusplus
}
#endif

