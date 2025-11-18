#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stddef.h>
#include <stdbool.h>
#include "driver/uart.h"

#define RS485_UART_PORT UART_NUM_1
#define RS485_TXD 15
#define RS485_RXD 16
#define RS485_RTS UART_PIN_NO_CHANGE

esp_err_t hal_rs485_init(void);
esp_err_t hal_rs485_send(const char *data);
esp_err_t hal_rs485_receive(char *buffer, size_t max_len, TickType_t ticks_to_wait);
bool hal_rs485_is_ready(void);
