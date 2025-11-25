#pragma once

#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom SDSPI host for Waveshare ESP32-S3-Touch-LCD-7B using CH422G EXIO4 as CS.
 */
sdmmc_host_t sdspi_host_ch422g_default(void);

esp_err_t sdspi_host_ch422g_init(void);
esp_err_t sdspi_host_ch422g_init_device(const sdspi_device_config_t* slot_config, sdspi_dev_handle_t* out_handle);
esp_err_t sdspi_host_ch422g_remove_device(sdspi_dev_handle_t handle);
esp_err_t sdspi_host_ch422g_deinit(void);
esp_err_t sdspi_ch422g_init_slot(spi_host_device_t host_id);
void sdspi_ch422g_deinit_slot(spi_host_device_t host_id);
esp_err_t sdspi_host_ch422g_set_card_clk(sdspi_dev_handle_t handle, uint32_t freq_khz);
esp_err_t sdspi_host_ch422g_do_transaction(int slot, sdmmc_command_t *cmdinfo);
esp_err_t sdspi_host_ch422g_io_int_enable(sdspi_dev_handle_t handle);
esp_err_t sdspi_host_ch422g_io_int_wait(sdspi_dev_handle_t handle, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
