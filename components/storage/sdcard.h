#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_SDCARD

typedef struct
{
    bool mounted;
    bool card_present;
    esp_err_t last_err;
} sdcard_status_t;

/**
 * @brief Initialize the SPI microSD interface and mount FATFS on /sdcard.
 *
 * Safe to call multiple times; the driver will only be initialized once.
 */
esp_err_t sdcard_init(void);

/**
 * @brief Return true if the card is currently mounted.
 */
bool sdcard_is_mounted(void);

/**
 * @brief Obtain the current status of the SD card subsystem.
 */
sdcard_status_t sdcard_get_status(void);

/**
 * @brief Optional runtime smoke test that writes and reads a file on /sdcard.
 */
esp_err_t sdcard_test_file(void);

#else

typedef struct
{
    bool mounted;
    bool card_present;
    esp_err_t last_err;
} sdcard_status_t;

static inline esp_err_t sdcard_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static inline bool sdcard_is_mounted(void)
{
    return false;
}

static inline sdcard_status_t sdcard_get_status(void)
{
    return (sdcard_status_t){
        .mounted = false,
        .card_present = false,
        .last_err = ESP_ERR_NOT_SUPPORTED,
    };
}

static inline esp_err_t sdcard_test_file(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // CONFIG_ENABLE_SDCARD

#ifdef __cplusplus
}
#endif
