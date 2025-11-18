#include "hal_sdcard.h"
#include "esp_log.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "hal_sdcard";
static sdmmc_card_t *mounted_card = NULL;
static bool sd_mounted = false;

static void hal_sdcard_set_cs(bool level)
{
    if (ch422g_init() == ESP_OK)
    {
        // Active low CS on EXIO4
        ch422g_set_pin(SD_SPI_CS_EXIO, level);
    }
    else
    {
        ESP_LOGW(TAG, "CH422G not ready for SD CS control");
    }
}

esp_err_t hal_sdcard_init(void)
{
    esp_err_t ret;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_SPI_MOSI,
        .miso_io_num = SD_SPI_MISO,
        .sclk_io_num = SD_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to init SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = -1; // Manual CS via CH422G EXIO4
    slot_config.host_id = SD_SPI_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    // Select card before mounting
    hal_sdcard_set_cs(false);
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &mounted_card);
    hal_sdcard_set_cs(true);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    sd_mounted = true;
    ESP_LOGI(TAG, "SD card mounted. Name: %s", mounted_card->cid.name);
    return ESP_OK;
}

bool hal_sdcard_is_mounted(void)
{
    return sd_mounted;
}

esp_err_t hal_sdcard_write_test(void)
{
    if (!sd_mounted)
    {
        return ESP_ERR_INVALID_STATE;
    }
    hal_sdcard_set_cs(false);
    FILE *f = fopen("/sdcard/test.txt", "w");
    if (!f)
    {
        hal_sdcard_set_cs(true);
        return ESP_FAIL;
    }
    const char *msg = "Hello from ESP32-S3 SD card!\n";
    size_t written = fwrite(msg, 1, strlen(msg), f);
    fclose(f);
    hal_sdcard_set_cs(true);
    return (written == strlen(msg)) ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_sdcard_read_test(char *out_buffer, size_t max_len)
{
    if (!sd_mounted)
    {
        return ESP_ERR_INVALID_STATE;
    }
    hal_sdcard_set_cs(false);
    FILE *f = fopen("/sdcard/test.txt", "r");
    if (!f)
    {
        hal_sdcard_set_cs(true);
        return ESP_FAIL;
    }
    size_t read_bytes = fread(out_buffer, 1, max_len - 1, f);
    fclose(f);
    hal_sdcard_set_cs(true);
    out_buffer[read_bytes] = '\0';
    return (read_bytes > 0) ? ESP_OK : ESP_FAIL;
}
