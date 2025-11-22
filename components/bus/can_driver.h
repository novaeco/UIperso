#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the onboard CAN (TWAI) controller.
 *
 * The function configures GPIOs, installs the TWAI driver, and starts it.
 */
esp_err_t can_bus_init(void);

/**
 * @brief Send a predefined CAN frame for quick diagnostics.
 */
esp_err_t can_bus_send_test_frame(void);

/**
 * @brief Receive a CAN frame if one is available.
 *
 * @param message        Pointer to store the incoming frame.
 * @param ticks_to_wait  Timeout while waiting for data.
 */
esp_err_t can_bus_receive_frame(twai_message_t *message, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

