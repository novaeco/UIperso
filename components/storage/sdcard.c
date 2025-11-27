#include "sdcard.h"

#if CONFIG_ENABLE_SDCARD

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"

#include "ch422g.h"
#include "sdspi_ch422g.h"

// Waveshare ESP32-S3 Touch LCD 7B: microSD over SPI with CS on CH422G EXIO4.
// IMPORTANT:
// - The real CS is I2C-driven (CH422G), so the stock SDSPI host (GPIO CS) cannot be used reliably.
// - Use the custom SDSPI host (sdspi_ch422g.c) which asserts/deasserts real CS via CH422G per command/transaction.
// - Mount SD *before* starting the RGB panel to avoid ISR-related watchdogs during GPIO/SPI setup.

#ifndef CONFIG_SDCARD_SPI_MOSI_GPIO
#define CONFIG_SDCARD_SPI_MOSI_GPIO  GPIO_NUM_11
#endif
#ifndef CONFIG_SDCARD_SPI_MISO_GPIO
#define CONFIG_SDCARD_SPI_MISO_GPIO  GPIO_NUM_13
#endif
#ifndef CONFIG_SDCARD_SPI_SCK_GPIO
#define CONFIG_SDCARD_SPI_SCK_GPIO   GPIO_NUM_12
#endif
#ifndef CONFIG_SDCARD_SPI_HOST
#define CONFIG_SDCARD_SPI_HOST       SPI2_HOST
#endif
#ifndef CONFIG_SDCARD_MAX_FILES
#define CONFIG_SDCARD_MAX_FILES      5
#endif

// Waveshare ESP32-S3 Touch LCD 7B µSD wiring:
// MOSI = GPIO11, MISO = GPIO13, SCK = GPIO12, CS = CH422G EXIO4 (active low)

#define SDCARD_MOUNT_POINT "/sdcard"

static const char *TAG = "SDCARD";

static sdcard_status_t s_status = {
    .mounted = false,
    .card_present = false,
    .last_err = ESP_OK,
};

typedef struct
{
    bool bus_initialized;
    bool bus_owned;
    bool slot_attached;
    spi_host_device_t host_id;
    sdmmc_card_t *card;
} sdcard_state_t;

static sdcard_state_t s_state = {
    .bus_initialized = false,
    .bus_owned = false,
    .slot_attached = false,
    .host_id = CONFIG_SDCARD_SPI_HOST,
    .card = NULL,
};

static void log_hw_config(spi_host_device_t host_id)
{
    ESP_LOGI(TAG,
             "SDSPI host=%d pins: MOSI=%d, MISO=%d, SCK=%d, CS=CH422G EXIO4 (active low), mount=%s, free_heap=%u",
             host_id,
             CONFIG_SDCARD_SPI_MOSI_GPIO,
             CONFIG_SDCARD_SPI_MISO_GPIO,
             CONFIG_SDCARD_SPI_SCK_GPIO,
             SDCARD_MOUNT_POINT,
             (unsigned)esp_get_free_heap_size());
}

static void sdcard_cleanup(sdcard_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    if (s_status.mounted && state->card)
    {
        ESP_LOGI(TAG, "Unmounting FATFS from %s", SDCARD_MOUNT_POINT);
        esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, state->card);
        s_status.mounted = false;
        s_status.card_present = false;
        state->card = NULL;
    }

    if (state->slot_attached)
    {
        ESP_LOGI(TAG, "Detaching SDSPI device on host %d", state->host_id);
        sdspi_ch422g_deinit_slot(state->host_id);
        state->slot_attached = false;
    }

    if (state->bus_initialized && state->bus_owned)
    {
        esp_err_t free_err = spi_bus_free(state->host_id);
        if (free_err != ESP_OK)
        {
            ESP_LOGW(TAG, "spi_bus_free(%d) failed during cleanup (%s)", state->host_id, esp_err_to_name(free_err));
        }
    }

    state->bus_initialized = false;
    state->bus_owned = false;

    (void)ch422g_set_sdcard_cs(false);
}

static bool sdcard_no_media_error(esp_err_t err)
{
    return (err == ESP_ERR_NOT_FOUND);
}

static void log_test_file_if_present(void)
{
    const char *test_path = SDCARD_MOUNT_POINT "/test.txt";
    struct stat st = {0};
    if (stat(test_path, &st) != 0)
    {
        ESP_LOGI(TAG, "Optional test file %s not found; skipping readback", test_path);
        return;
    }

    FILE *f = fopen(test_path, "r");
    if (f == NULL)
    {
        ESP_LOGW(TAG, "Found %s but could not open for read", test_path);
        return;
    }

    char buffer[128] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    ESP_LOGI(TAG, "Read %zu byte(s) from %s: '%s'", read, test_path, buffer);
}

static esp_err_t sdcard_mount(void)
{
    if (s_status.mounted)
    {
        return ESP_OK;
    }

    s_status.last_err = ESP_OK;

    if (!ch422g_sdcard_cs_available())
    {
        ESP_LOGE(TAG, "CH422G not ready; cannot drive SD CS (EXIO4)");
        s_status.last_err = ESP_ERR_INVALID_STATE;
        return s_status.last_err;
    }

    const spi_host_device_t host_id = CONFIG_SDCARD_SPI_HOST;
    s_state.host_id = host_id;

    log_hw_config(host_id);

    const gpio_num_t pull_pins[] = {
        CONFIG_SDCARD_SPI_MISO_GPIO,
        CONFIG_SDCARD_SPI_MOSI_GPIO,
        CONFIG_SDCARD_SPI_SCK_GPIO,
    };

    for (size_t i = 0; i < sizeof(pull_pins) / sizeof(pull_pins[0]); ++i)
    {
        if (pull_pins[i] == GPIO_NUM_NC)
        {
            continue;
        }
        esp_err_t pull_err = gpio_set_pull_mode(pull_pins[i], GPIO_PULLUP_ONLY);
        if (pull_err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to enable pull-up on GPIO%d (%s)", pull_pins[i], esp_err_to_name(pull_err));
        }
    }

    esp_err_t ret = ch422g_set_sdcard_cs(false);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deassert SD CS via CH422G (%s)", esp_err_to_name(ret));
        s_status.last_err = ret;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    spi_bus_config_t bus_config = {
        .mosi_io_num = CONFIG_SDCARD_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_SDCARD_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_SDCARD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = 0,
        .intr_flags = 0,
    };

    ret = spi_bus_initialize(host_id, &bus_config, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "SPI bus already initialized, reusing");
        s_state.bus_initialized = true;
        s_state.bus_owned = false;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init SPI bus (%s)", esp_err_to_name(ret));
        s_status.last_err = ret;
        goto fail;
    }
    else
    {
        s_state.bus_initialized = true;
        s_state.bus_owned = true;
    }

    ESP_LOGI(TAG, "Sending idle clocks on host %d before mount", host_id);
    ret = sdspi_ch422g_idle_clocks(host_id);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to send idle clocks before mount (%s)", esp_err_to_name(ret));
    }

    sdmmc_host_t host = sdspi_host_ch422g_default();
    host.slot = host_id;
    host.max_freq_khz = 400;

    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.host_id = host.slot;
    dev_cfg.gpio_cs  = GPIO_NUM_NC;
    dev_cfg.gpio_cd  = GPIO_NUM_NC;
    dev_cfg.gpio_wp  = GPIO_NUM_NC;
    dev_cfg.gpio_int = GPIO_NUM_NC;

    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = CONFIG_SDCARD_MAX_FILES,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = true,
    };

    sdmmc_card_t *card = NULL;

    ESP_LOGI(TAG, "esp_vfs_fat_sdspi_mount start (custom CH422G CS host)");
    ret = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &dev_cfg, &mount_config, &card);
    ESP_LOGI(TAG, "esp_vfs_fat_sdspi_mount result=%s", esp_err_to_name(ret));

    s_state.slot_attached = true;

    if (ret != ESP_OK)
    {
        if (sdcard_no_media_error(ret))
        {
            ESP_LOGW(TAG, "No SD card detected on %s; continuing without storage", SDCARD_MOUNT_POINT);
        }
        else if (ret == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(TAG, "microSD init timed out (%s); card may be missing or not responding; continuing without storage", esp_err_to_name(ret));
        }
        else if (ret == ESP_ERR_INVALID_RESPONSE || ret == ESP_FAIL)
        {
            ESP_LOGW(TAG, "microSD init failed, card not responding correctly (%s); continuing without storage", esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGE(TAG, "Failed to mount %s (%s)", SDCARD_MOUNT_POINT, esp_err_to_name(ret));
        }
        s_status.last_err = ret;
        s_status.card_present = false;
        goto fail;
    }

    s_status.mounted = true;
    s_status.card_present = true;
    s_status.last_err = ESP_OK;
    s_state.card = card;

    esp_err_t freq_ret = sdspi_host_ch422g_set_card_clk((sdspi_dev_handle_t)host_id, 20000);
    if (freq_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to raise SD clock to 20 MHz (%s)", esp_err_to_name(freq_ret));
    }

    sdmmc_card_print_info(stdout, card);
    log_test_file_if_present();
    ESP_LOGI(TAG, "microSD mounted OK on %s", SDCARD_MOUNT_POINT);
    return ESP_OK;

fail:
    sdcard_cleanup(&s_state);
    return s_status.last_err;
}

esp_err_t sdcard_init(void)
{
    return sdcard_mount();
}

bool sdcard_is_mounted(void)
{
    return s_status.mounted;
}

sdcard_status_t sdcard_get_status(void)
{
    return s_status;
}

esp_err_t sdcard_test_file(void)
{
    if (!s_status.mounted)
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

#endif // CONFIG_ENABLE_SDCARD
