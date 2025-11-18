#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include "driver/twai.h"

#define CAN_TX_GPIO 20
#define CAN_RX_GPIO 19

esp_err_t hal_can_init(void);
esp_err_t hal_can_send_test_frame(void);
esp_err_t hal_can_receive(twai_message_t *out_message, TickType_t ticks_to_wait);
bool hal_can_is_started(void);
