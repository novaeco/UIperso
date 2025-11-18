#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "hal_ioexp_ch422g.h"

#define SD_SPI_HOST SPI3_HOST
#define SD_SPI_MISO 13
#define SD_SPI_MOSI 11
#define SD_SPI_CLK 12
#define SD_SPI_CS_EXIO CH422G_EXIO4

esp_err_t hal_sdcard_init(void);
bool hal_sdcard_is_mounted(void);
esp_err_t hal_sdcard_write_test(void);
esp_err_t hal_sdcard_read_test(char *out_buffer, size_t max_len);
