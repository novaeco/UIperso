#include "sdcard.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "ch422g.h"

#ifndef CONFIG_SDCARD_SPI_MOSI_GPIO
#define CONFIG_SDCARD_SPI_MOSI_GPIO  GPIO_NUM_11  // Fallback wiring if IO extension unavailable
#endif
#ifndef CONFIG_SDCARD_SPI_MISO_GPIO
#define CONFIG_SDCARD_SPI_MISO_GPIO  GPIO_NUM_13  // Fallback wiring if IO extension unavailable
#endif
#ifndef CONFIG_SDCARD_SPI_SCK_GPIO
#define CONFIG_SDCARD_SPI_SCK_GPIO   GPIO_NUM_12  // Fallback wiring if IO extension unavailable
#endif
#ifndef CONFIG_SDCARD_SPI_CS_GPIO
#define CONFIG_SDCARD_SPI_CS_GPIO    GPIO_NUM_10  // Fallback wiring if IO extension unavailable
#endif

#define SDCARD_MOUNT_POINT "/sdcard"

static const char *TAG = "SDCARD";

static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;

static esp_err_t sdcard_mount(void)
{
    if (s_mounted)
    {
        return ESP_OK;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST; // ESP32-S3: SPI3 (VSPI) disponible pour le slot µSD.

    spi_bus_config_t bus_config = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = 0,
        .intr_flags = 0,
    };

    bool bus_initialized_here = false;
    esp_err_t err = spi_bus_initialize(host.slot, &bus_config, SPI_DMA_CH_AUTO);
    if (err == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "SPI bus already initialized, reusing");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init SPI bus (%s)", esp_err_to_name(err));
        return err;
    }
    else
    {
        bus_initialized_here = true;
    }

    sdmmc_card_t *card = NULL;

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = true,
    };

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    // Keep the CS line released through the IO expander if available so the
    // card is not spuriously selected when the SPI bus comes up. The actual CS
    // toggling during SPI transactions is handled by the GPIO configured below.
    if (ch422g_sdcard_cs_available())
    {
        esp_err_t ch_err = ch422g_set_sdcard_cs(false);
        if (ch_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Unable to deassert SD CS via IO extension (%s), continuing", esp_err_to_name(ch_err));
        }
    }

    slot_config.gpio_cs = CONFIG_SDCARD_SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    // Treat "no card" as a normal situation: reduce the log level of the
    // VFS/SDMMC helper during probing to avoid alarming E-level messages when
    // the slot is empty, then restore it afterwards.
    const esp_log_level_t prev_vfs_level = esp_log_level_get("vfs_fat_sdmmc");
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_WARN);

    err = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    esp_log_level_set("vfs_fat_sdmmc", prev_vfs_level);

    if (err != ESP_OK)
    {
        if (err == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(TAG, "No SD card detected on %s; continuing without storage", SDCARD_MOUNT_POINT);
        }
        else
        {
            ESP_LOGW(TAG, "Failed to mount %s (%s)", SDCARD_MOUNT_POINT, esp_err_to_name(err));
        }
        if (bus_initialized_here)
        {
            spi_bus_free(host.slot);
        }
        return err;
    }

    s_card = card;
    s_mounted = true;

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "Mounted %s successfully", SDCARD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t sdcard_init(void)
{
    return sdcard_mount();
}

bool sdcard_is_mounted(void)
{
    return s_mounted;
}

esp_err_t sdcard_test_file(void)
{
    if (!s_mounted)
    {
        ESP_LOGW(TAG, "Cannot run sdcard_test_file: card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    static const char *test_path = SDCARD_MOUNT_POINT "/test.txt";
    static const char *payload = "ESP32-S3 storage test\n";

    FILE *f = fopen(test_path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open %s for writing", test_path);
        return ESP_FAIL;
    }

    size_t written = fwrite(payload, 1, strlen(payload), f);
    fclose(f);

    if (written != strlen(payload))
    {
        ESP_LOGE(TAG, "Short write on %s (%zu/%zu)", test_path, written, strlen(payload));
        return ESP_FAIL;
    }

    f = fopen(test_path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open %s for reading", test_path);
        return ESP_FAIL;
    }

    char buffer[64] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);

    if (read != strlen(payload))
    {
        ESP_LOGE(TAG, "Unexpected length read (%zu/%zu)", read, strlen(payload));
        return ESP_FAIL;
    }

    if (strncmp(buffer, payload, strlen(payload)) != 0)
    {
        ESP_LOGE(TAG, "Content mismatch: %s", buffer);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "sdcard_test_file succeeded (%s)", test_path);
    return ESP_OK;
}
