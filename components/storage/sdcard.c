#include "sdcard.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "ch422g.h"

// Waveshare ESP32-S3 Touch LCD 7B: microSD over SPI with CS on CH422G EXIO4.
// CS is driven manually through the IO expander; the VFS layer sees a "dummy" CS GPIO.

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
#define CONFIG_SDCARD_SPI_HOST       SPI2_HOST    // Default to FSPI on ESP32-S3 (host=2)
#endif
#ifndef CONFIG_SDCARD_MAX_FILES
#define CONFIG_SDCARD_MAX_FILES      5            // Conservative default if Kconfig entry is absent
#endif

// Dummy CS pin required by esp_vfs_fat_sdspi_mount, not connected on hardware.
// GPIO33 is free on the Waveshare ESP32-S3 Touch LCD 7B and can be driven safely.
#define SDCARD_DUMMY_CS_GPIO GPIO_NUM_33

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

    if (!ch422g_sdcard_cs_available())
    {
        ESP_LOGE(TAG, "CH422G not ready; cannot drive SD CS (EXIO4)");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure the dummy CS GPIO to satisfy the SDSPI API; it is not wired to the card.
    const gpio_config_t dummy_cs_cfg = {
        .pin_bit_mask = 1ULL << SDCARD_DUMMY_CS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&dummy_cs_cfg), TAG, "Failed to init dummy CS GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(SDCARD_DUMMY_CS_GPIO, 1), TAG, "Failed to set dummy CS high");

    // Force the real CS low via CH422G before probing the card.
    ESP_RETURN_ON_ERROR(ch422g_set_sdcard_cs(true), TAG, "Failed to assert SD CS via CH422G");
    vTaskDelay(pdMS_TO_TICKS(5));

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = CONFIG_SDCARD_SPI_HOST; // ESP32-S3: SPI2 (FSPI) disponible pour le slot µSD.
    host.max_freq_khz = 20000;          // Conservative 20 MHz for stable bring-up

    spi_bus_config_t bus_config = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    bool bus_initialized_here = false;
    ESP_LOGI(TAG, "SDSPI config (CH422G CS): host=%d, mosi=%d, miso=%d, sck=%d, exio_cs=EXIO4, dummy_cs=%d",
             host.slot, CONFIG_SDCARD_SPI_MOSI_GPIO, CONFIG_SDCARD_SPI_MISO_GPIO, CONFIG_SDCARD_SPI_SCK_GPIO,
             SDCARD_DUMMY_CS_GPIO);

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
    slot_config.gpio_cs = SDCARD_DUMMY_CS_GPIO; // satisfies the API; real CS is EXIO4 via CH422G

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
        ch422g_set_sdcard_cs(false);
        if (bus_initialized_here)
        {
            spi_bus_free(host.slot);
        }
        return err;
    }

    s_card = card;
    s_mounted = true;

    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "microSD mounted OK on %s", SDCARD_MOUNT_POINT);
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
