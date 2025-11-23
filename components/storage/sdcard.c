#include "sdcard.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
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
#ifndef CONFIG_SDCARD_SPI_HOST
#define CONFIG_SDCARD_SPI_HOST       SPI3_HOST    // Default to VSPI on ESP32-S3
#endif
#ifndef CONFIG_SDCARD_MAX_FILES
#define CONFIG_SDCARD_MAX_FILES      5            // Conservative default if Kconfig entry is absent
#endif

// Waveshare ESP32-S3 Touch LCD 7B µSD wiring (SPI via CH422G):
// MOSI = GPIO11, MISO = GPIO13, SCK = GPIO12, CS = EXIO4 (active low)

#define SDCARD_MOUNT_POINT "/sdcard"

static const char *TAG = "SDCARD";

static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;

static bool sdcard_no_media_error(esp_err_t err)
{
    return (err == ESP_ERR_NOT_FOUND);
}

static esp_err_t sdcard_mount(void)
{
    if (s_mounted)
    {
        return ESP_OK;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = CONFIG_SDCARD_SPI_HOST; // ESP32-S3: SPI3 (VSPI) disponible pour le slot µSD.
    host.max_freq_khz = 10000;          // Debug-friendly frequency for better interoperability

    spi_bus_config_t bus_config = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    bool bus_initialized_here = false;
    bool cs_forced_high = false;

    ESP_LOGI(TAG, "SDSPI config: host=%d, mosi=%d, miso=%d, sck=%d, cs=%d", host.slot,
             CONFIG_SDCARD_SPI_MOSI_GPIO, CONFIG_SDCARD_SPI_MISO_GPIO, CONFIG_SDCARD_SPI_SCK_GPIO,
             ch422g_sdcard_cs_available() ? -1 : CONFIG_SDCARD_SPI_CS_GPIO);

    if (ch422g_sdcard_cs_available())
    {
        ESP_LOGI(TAG, "SD CS controlled via CH422G EXIO4 (not a direct GPIO); make sure it is driven low during transactions");
    }

    esp_err_t err = spi_bus_initialize(host.slot, &bus_config, SDSPI_DEFAULT_DMA);
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

    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = CONFIG_SDCARD_MAX_FILES,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = true,
    };

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = host.slot;

    // CS is routed through CH422G EXIO4. Keep it permanently asserted (active low)
    // so the dedicated SPI device remains selected; the SDSPI driver will still
    // use the configured chip-select GPIO for standard wiring scenarios.
    if (ch422g_sdcard_cs_available())
    {
        esp_err_t ch_err = ch422g_set_sdcard_cs(true);
        if (ch_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Unable to assert SD CS via IO extension (%s), continuing", esp_err_to_name(ch_err));
        }
        else
        {
            cs_forced_high = true;
        }
    }

    slot_config.gpio_cs = CONFIG_SDCARD_SPI_CS_GPIO;

    err = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(err));
    }

    if (err != ESP_OK)
    {
        if (sdcard_no_media_error(err))
        {
            ESP_LOGW(TAG, "No SD card detected on %s; continuing without storage", SDCARD_MOUNT_POINT);
        }
        else if (err == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(TAG, "microSD init timed out (%s); card may be missing or not responding; continuing without storage", esp_err_to_name(err));
        }
        else if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_FAIL)
        {
            ESP_LOGW(TAG, "microSD init failed, card not responding correctly (%s); continuing without storage", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGE(TAG, "Failed to mount %s (%s)", SDCARD_MOUNT_POINT, esp_err_to_name(err));
        }
        if (bus_initialized_here)
        {
            spi_bus_free(host.slot);
        }
        if (cs_forced_high)
        {
            ch422g_set_sdcard_cs(false);
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
