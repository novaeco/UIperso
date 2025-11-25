/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Custom SDSPI host driver adapted for Waveshare ESP32-S3-Touch-LCD-7B.
// The microSD chip-select is routed to CH422G EXIO4 (I²C-controlled), not a native ESP32-S3 GPIO.
// CS handling is fully managed here (idle clocks with CS high, assert/deassert around each command)
// while keeping compatibility with esp_vfs_fat_sdspi_mount.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/param.h>

#include "esp_log.h"
#include "esp_log_buffer.h" // esp_log_buffer_hex lives here; required for C23/-Werror builds
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "sys/lock.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/sdmmc_defs.h"
#include "driver/sdspi_host.h"

#include "sdspi_private.h"
#include "sdspi_crc.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "soc/soc_memory_layout.h"

#include "ch422g.h"
#include "sdspi_ch422g.h"

// Compatibilité API ESP-IDF 5+ : sdspi_slot_config_t a été supprimé,
// on le redéfinit localement pour ce driver custom (uniquement pour l'API deprecated init_slot()).
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
typedef struct {
    gpio_num_t gpio_cs;    // CS signal
    gpio_num_t gpio_cd;    // Card Detect
    gpio_num_t gpio_wp;    // Write Protect
    gpio_num_t gpio_int;   // SDIO interrupt line
    gpio_num_t gpio_miso;  // MISO signal
    gpio_num_t gpio_mosi;  // MOSI signal
    gpio_num_t gpio_sck;   // SCK signal
    int        dma_channel;
} sdspi_slot_config_t;
#endif

/// Max number of transactions in flight (used in start_command_write_blocks)
#define SDSPI_TRANSACTION_COUNT 4
#define SDSPI_MOSI_IDLE_VAL     0xff    //!< Data value which causes MOSI to stay high
#define GPIO_UNUSED             0xff    //!< Flag indicating that CD/WP is unused
/// Size of the buffer returned by get_block_buf
#define SDSPI_BLOCK_BUF_SIZE    (SDSPI_MAX_DATA_LEN + 4)
/// Maximum number of dummy bytes between the request and response (minimum is 1)
#define SDSPI_RESPONSE_MAX_DELAY  8

typedef struct {
    spi_host_device_t    host_id;     //!< SPI host id.
    spi_device_handle_t  spi_handle;  //!< SPI device handle, used for transactions
    uint8_t gpio_cs;                 //!< Unused (CS is CH422G)
    uint8_t gpio_cd;                 //!< Card detect GPIO, or GPIO_UNUSED
    uint8_t gpio_wp;                 //!< Write protect GPIO, or GPIO_UNUSED
    uint8_t gpio_int;                //!< SDIO interrupt GPIO, or GPIO_UNUSED
    uint8_t data_crc_enabled : 1;    //!< CRC enabled by upper layer
    uint8_t *block_buf;              //!< DMA intermediate buffer (lazy alloc)
    SemaphoreHandle_t semphr_int;     //!< Unused (no SDIO IRQ line)
} slot_info_t;

// Reserved for old API to be back-compatible
static slot_info_t *s_slots[SOC_SPI_PERIPH_NUM] = {};
static const char *TAG = "sdspi_ch422g";

static const bool use_polling = true;
static const bool no_use_polling = false;
static _lock_t s_lock;
static bool s_app_cmd;
static bool s_logged_cs_assert;
static spi_device_handle_t s_sd_dev = NULL;
static spi_host_device_t s_sd_host = SPI1_HOST;

static uint8_t sdspi_msg_crc7_ch422g(sdspi_hw_cmd_t* hw_cmd)
{
    const size_t bytes_to_crc = offsetof(sdspi_hw_cmd_t, arguments) + sizeof(hw_cmd->arguments);
    return sdspi_crc7((const uint8_t *)hw_cmd, bytes_to_crc);
}

static void make_hw_cmd_ch422g(uint32_t opcode, uint32_t arg, int timeout_ms, sdspi_hw_cmd_t *hw_cmd)
{
    hw_cmd->start_bit = 0;
    hw_cmd->transmission_bit = 1;
    hw_cmd->cmd_index = opcode;
    hw_cmd->stop_bit = 1;
    hw_cmd->r1 = 0xff;
    memset(hw_cmd->response, 0xff, sizeof(hw_cmd->response));
    hw_cmd->ncr = 0xff;
    uint32_t arg_s = __builtin_bswap32(arg);
    memcpy(hw_cmd->arguments, &arg_s, sizeof(arg_s));
    hw_cmd->crc7 = sdspi_msg_crc7_ch422g(hw_cmd);
    hw_cmd->timeout_ms = timeout_ms;
}

static void r1_response_to_err(uint8_t r1, int cmd, esp_err_t *out_err)
{
    if (r1 & SD_SPI_R1_NO_RESPONSE) {
        ESP_LOGD(TAG, "cmd=%d, R1 response not found", cmd);
        *out_err = ESP_ERR_TIMEOUT;
    } else if (r1 & SD_SPI_R1_CMD_CRC_ERR) {
        ESP_LOGD(TAG, "cmd=%d, R1 response: command CRC error", cmd);
        *out_err = ESP_ERR_INVALID_CRC;
    } else if (r1 & SD_SPI_R1_ILLEGAL_CMD) {
        ESP_LOGD(TAG, "cmd=%d, R1 response: command not supported", cmd);
        *out_err = ESP_ERR_NOT_SUPPORTED;
    } else if (r1 & SD_SPI_R1_ADDR_ERR) {
        ESP_LOGD(TAG, "cmd=%d, R1 response: alignment error", cmd);
        *out_err = ESP_ERR_INVALID_ARG;
    } else if (r1 & SD_SPI_R1_PARAM_ERR) {
        ESP_LOGD(TAG, "cmd=%d, R1 response: size error", cmd);
        *out_err = ESP_ERR_INVALID_SIZE;
    } else if ((r1 & SD_SPI_R1_ERASE_RST) || (r1 & SD_SPI_R1_ERASE_SEQ_ERR)) {
        *out_err = ESP_ERR_INVALID_STATE;
    } else if (r1 & SD_SPI_R1_IDLE_STATE) {
        // Idle state is handled at command layer
    } else if (r1 != 0) {
        ESP_LOGD(TAG, "cmd=%d, R1 response: unexpected value 0x%02x", cmd, r1);
        *out_err = ESP_ERR_INVALID_RESPONSE;
    }
}

static void r1_sdio_response_to_err(uint8_t r1, int cmd, esp_err_t *out_err)
{
    if (r1 & SD_SPI_R1_NO_RESPONSE) {
        ESP_LOGI(TAG, "cmd=%d, R1 response not found", cmd);
        *out_err = ESP_ERR_TIMEOUT;
    } else if (r1 & SD_SPI_R1_CMD_CRC_ERR) {
        ESP_LOGI(TAG, "cmd=%d, R1 response: command CRC error", cmd);
        *out_err = ESP_ERR_INVALID_CRC;
    } else if (r1 & SD_SPI_R1_ILLEGAL_CMD) {
        ESP_LOGI(TAG, "cmd=%d, R1 response: command not supported", cmd);
        *out_err = ESP_ERR_NOT_SUPPORTED;
    } else if (r1 & SD_SPI_R1_PARAM_ERR) {
        ESP_LOGI(TAG, "cmd=%d, R1 response: size error", cmd);
        *out_err = ESP_ERR_NOT_SUPPORTED;
    } else if (r1 & SDIO_R1_FUNC_NUM_ERR) {
        ESP_LOGI(TAG, "cmd=%d, R1 response: function number error", cmd);
        *out_err = ESP_ERR_NOT_SUPPORTED;
    } else if (r1 & SD_SPI_R1_IDLE_STATE) {
        // Idle state is handled at command layer
    } else if (r1 != 0) {
        ESP_LOGI(TAG, "cmd=%d, R1 response: unexpected value 0x%02x", cmd, r1);
        *out_err = ESP_ERR_INVALID_RESPONSE;
    }
}

static esp_err_t start_command_read_blocks(slot_info_t *slot, sdspi_hw_cmd_t *cmd,
        uint8_t *data, uint32_t rx_length, bool need_stop_command);

static esp_err_t start_command_write_blocks(slot_info_t *slot, sdspi_hw_cmd_t *cmd,
        const uint8_t *data, uint32_t tx_length, bool multi_block, bool stop_trans);

static esp_err_t start_command_default(slot_info_t *slot, int flags, sdspi_hw_cmd_t *cmd);
static esp_err_t shift_cmd_response(sdspi_hw_cmd_t *cmd, int sent_bytes);

static slot_info_t* get_slot_info(sdspi_dev_handle_t handle)
{
    if ((uint32_t) handle < SOC_SPI_PERIPH_NUM) {
        return s_slots[handle];
    } else {
        return (slot_info_t *) handle;
    }
}

static sdspi_dev_handle_t store_slot_info(slot_info_t *slot)
{
    if (s_slots[slot->host_id] == NULL) {
        s_slots[slot->host_id] = slot;
        return slot->host_id;
    } else {
        return (sdspi_dev_handle_t)slot;
    }
}

static slot_info_t* remove_slot_info(sdspi_dev_handle_t handle)
{
    if ((uint32_t) handle < SOC_SPI_PERIPH_NUM) {
        slot_info_t* slot = s_slots[handle];
        s_slots[handle] = NULL;
        return slot;
    } else {
        return (slot_info_t *) handle;
    }
}

/// Set CS high for given slot using CH422G (EXIO4 is active low)
static esp_err_t cs_high(slot_info_t *slot)
{
    (void)slot;
    esp_err_t err = ch422g_set_sdcard_cs(false);
    ESP_LOGD(TAG, "CS -> HIGH (release), rc=%s", esp_err_to_name(err));
    return err;
}

/// Set CS low for given slot using CH422G (EXIO4 is active low)
static esp_err_t cs_low(slot_info_t *slot)
{
    (void)slot;
    esp_err_t err = ch422g_set_sdcard_cs(true);
    ESP_LOGD(TAG, "CS -> LOW (assert), rc=%s", esp_err_to_name(err));
    if (!s_logged_cs_assert && err == ESP_OK) {
        ESP_LOGI(TAG, "CS asserted via EXIO4 (idx=%d)", 4);
        s_logged_cs_assert = true;
    }
    // CS toggles through I2C → CH422G, give it a few microseconds to settle
    esp_rom_delay_us(5);
    return err;
}

static bool card_write_protected(slot_info_t *slot)
{
    (void)slot;
    return false;
}

static bool card_missing(slot_info_t *slot)
{
    (void)slot;
    return false;
}

static esp_err_t get_block_buf(slot_info_t *slot, uint8_t **out_buf)
{
    if (slot->block_buf == NULL) {
        slot->block_buf = heap_caps_malloc(SDSPI_BLOCK_BUF_SIZE, MALLOC_CAP_DMA);
        if (slot->block_buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    *out_buf = slot->block_buf;
    return ESP_OK;
}

static void release_bus(slot_info_t *slot)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = {0xff}
    };
    (void)spi_device_polling_transmit(slot->spi_handle, &t);
}

static esp_err_t go_idle_clockout(slot_info_t *slot)
{
    uint8_t data[12];
    memset(data, 0xff, sizeof(data));
    spi_transaction_t t = {
        .length = 10 * 8,
        .tx_buffer = data,
        .rx_buffer = data,
    };

    esp_err_t ret = spi_device_polling_transmit(slot->spi_handle, &t);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send 80 clocks with CS high before CMD0 (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "MISO during idle clocks (first 10 bytes)");
#if CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG
    ESP_LOG_BUFFER_HEX(TAG, data, 10);
#endif
    return ESP_OK;
}

static esp_err_t configure_spi_dev(slot_info_t *slot, int clock_speed_hz)
{
    if (slot->spi_handle) {
        esp_err_t rem_ret = spi_bus_remove_device(slot->spi_handle);
        if (rem_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove SPI device before reconfig (%s)", esp_err_to_name(rem_ret));
            return rem_ret;
        }
        slot->spi_handle = NULL;
    }
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = clock_speed_hz,
        .mode = 0,
        // CS is controlled externally via CH422G (I2C), not by the SPI peripheral
        .spics_io_num = GPIO_NUM_NC,
        .queue_size = SDSPI_TRANSACTION_COUNT,
    };
    return spi_bus_add_device(slot->host_id, &devcfg, &slot->spi_handle);
}

esp_err_t sdspi_host_ch422g_init(void)
{
    return ESP_OK;
}

static esp_err_t ensure_slot_initialized(int host_id, slot_info_t **out_slot)
{
    slot_info_t *slot = get_slot_info((sdspi_dev_handle_t)host_id);
    if (slot != NULL) {
        *out_slot = slot;
        return ESP_OK;
    }

    if (!ch422g_sdcard_cs_available()) {
        ESP_LOGE(TAG, "CH422G unavailable; cannot create SD slot");
        return ESP_ERR_INVALID_STATE;
    }

    slot = calloc(1, sizeof(slot_info_t));
    if (slot == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *slot = (slot_info_t) {
        .host_id = host_id,
        .gpio_cs = GPIO_UNUSED,
        .gpio_cd = GPIO_UNUSED,
        .gpio_wp = GPIO_UNUSED,
        .gpio_int = GPIO_UNUSED,
        .semphr_int = NULL,
        .block_buf = NULL,
        .data_crc_enabled = 0,
        .spi_handle = NULL,
    };

    esp_err_t ret = configure_spi_dev(slot, SDMMC_FREQ_PROBING * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device for host %d (%s)", host_id, esp_err_to_name(ret));
        free(slot);
        return ret;
    }

    ret = cs_high(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release SD CS via CH422G during slot init (%s)", esp_err_to_name(ret));
        spi_bus_remove_device(slot->spi_handle);
        free(slot);
        return ret;
    }

    (void)store_slot_info(slot);
    *out_slot = slot;
    ESP_LOGI(TAG, "Slot %d initialized for CH422G-controlled SDSPI", host_id);
    return ESP_OK;
}

esp_err_t sdspi_ch422g_init_slot(spi_host_device_t host_id)
{
    slot_info_t *slot = NULL;
    esp_err_t ret = ensure_slot_initialized(host_id, &slot);
    if (ret != ESP_OK) {
        return ret;
    }

    s_sd_host = host_id;
    s_sd_dev = slot->spi_handle;
    return ESP_OK;
}

static esp_err_t deinit_slot(slot_info_t *slot)
{
    if (slot->spi_handle) {
        esp_err_t rem_ret = spi_bus_remove_device(slot->spi_handle);
        if (rem_ret != ESP_OK) {
            ESP_LOGW(TAG, "spi_bus_remove_device failed during deinit (%s)", esp_err_to_name(rem_ret));
        } else {
            slot->spi_handle = NULL;
        }
    }

    free(slot->block_buf);
    slot->block_buf = NULL;

    (void)cs_high(slot);
    free(slot);
    return ESP_OK;
}

esp_err_t sdspi_host_ch422g_remove_device(sdspi_dev_handle_t handle)
{
    slot_info_t* slot = remove_slot_info(handle);
    if (slot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_sd_dev == slot->spi_handle) {
        s_sd_dev = NULL;
    }
    return deinit_slot(slot);
}

esp_err_t sdspi_host_ch422g_deinit(void)
{
    for (size_t i = 0; i < sizeof(s_slots)/sizeof(s_slots[0]); ++i) {
        slot_info_t* slot = remove_slot_info(i);
        if (slot == NULL) continue;
        (void)deinit_slot(slot);
    }
    s_sd_dev = NULL;
    return ESP_OK;
}

void sdspi_ch422g_deinit_slot(spi_host_device_t host_id)
{
    slot_info_t *slot = get_slot_info(host_id);
    if (slot != NULL && slot->spi_handle != NULL && s_sd_dev == slot->spi_handle && s_sd_host == host_id)
    {
        esp_err_t rem_ret = spi_bus_remove_device(slot->spi_handle);
        if (rem_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "spi_bus_remove_device failed during slot deinit (%s)", esp_err_to_name(rem_ret));
        }
        slot->spi_handle = NULL;
        s_sd_dev = NULL;
    }

    slot = remove_slot_info(host_id);
    if (slot != NULL)
    {
        (void)deinit_slot(slot);
    }
}

esp_err_t sdspi_host_ch422g_set_card_clk(sdspi_dev_handle_t handle, uint32_t freq_khz)
{
    slot_info_t *slot = get_slot_info(handle);
    if (slot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Setting card clock to %d kHz", freq_khz);
    return configure_spi_dev(slot, (int)freq_khz * 1000);
}

esp_err_t sdspi_host_ch422g_init_device(const sdspi_device_config_t* slot_config, sdspi_dev_handle_t* out_handle)
{
    ESP_LOGI(TAG, "%s: SPI%d with CH422G-controlled CS (gpio_cs=%d)",
             __func__, slot_config->host_id + 1, slot_config->gpio_cs);

    if (!ch422g_sdcard_cs_available()) {
        ESP_LOGE(TAG, "CH422G unavailable; cannot control SD CS");
        return ESP_ERR_INVALID_STATE;
    }

    slot_info_t* slot = (slot_info_t*)malloc(sizeof(slot_info_t));
    if (slot == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *slot = (slot_info_t) {
        .host_id = slot_config->host_id,
        .gpio_cs = GPIO_UNUSED,
        .gpio_cd = GPIO_UNUSED,
        .gpio_wp = GPIO_UNUSED,
        .gpio_int = GPIO_UNUSED,
        .semphr_int = NULL,
        .block_buf = NULL,
        .data_crc_enabled = 0,
        .spi_handle = NULL,
    };

    esp_err_t ret = configure_spi_dev(slot, SDMMC_FREQ_PROBING * 1000);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "spi_bus_add_device failed with rc=0x%x", ret);
        free(slot);
        return ret;
    }

    // Ensure CS is released before any GO_IDLE clocking.
    ret = cs_high(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release SD CS via CH422G (%s)", esp_err_to_name(ret));
        spi_bus_remove_device(slot->spi_handle);
        free(slot);
        return ret;
    }

    *out_handle = store_slot_info(slot);
    return ESP_OK;
}

esp_err_t sdspi_host_ch422g_start_command(sdspi_dev_handle_t handle, sdspi_hw_cmd_t *cmd, void *data,
                                         uint32_t data_size, int flags)
{
    slot_info_t *slot = get_slot_info(handle);
    if (slot == NULL) {
        esp_err_t init_ret = ensure_slot_initialized((int)handle, &slot);
        if (init_ret != ESP_OK) {
            return init_ret;
        }
    }
    if (!ch422g_sdcard_cs_available()) {
        ESP_LOGE(TAG, "CH422G not available; cannot start SD command");
        return ESP_ERR_INVALID_STATE;
    }
    if (card_missing(slot)) {
        return ESP_ERR_NOT_FOUND;
    }

    int cmd_index = cmd->cmd_index;
    uint32_t cmd_arg;
    memcpy(&cmd_arg, cmd->arguments, sizeof(cmd_arg));
    cmd_arg = __builtin_bswap32(cmd_arg);

    ESP_LOGV(TAG, "%s: slot=%i, CMD%d, arg=0x%08x flags=0x%x, data=%p, data_size=%i crc=0x%02x",
             __func__, handle, cmd_index, cmd_arg, flags, data, data_size, cmd->crc7);

    esp_err_t ret = spi_device_acquire_bus(slot->spi_handle, portMAX_DELAY);
    bool bus_acquired = (ret == ESP_OK);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire SPI bus (%s)", esp_err_to_name(ret));
        return ret;
    }

    // Ensure CS is released before any GO_IDLE clocking.
    ret = cs_high(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release SD CS via CH422G (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    // For CMD0, clock out 80 cycles with CS high before asserting CS and sending the command.
    if (cmd_index == MMC_GO_IDLE_STATE) {
        ret = go_idle_clockout(slot);
        if (ret != ESP_OK) {
            goto cleanup;
        }
    }

    ret = cs_low(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to assert SD CS via CH422G (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    if (flags & SDSPI_CMD_FLAG_DATA) {
        const bool multi_block = flags & SDSPI_CMD_FLAG_MULTI_BLK;
        const bool stop_transmission = multi_block && !(flags & SDSPI_CMD_FLAG_RSP_R5);
        if (flags & SDSPI_CMD_FLAG_WRITE) {
            ret = start_command_write_blocks(slot, cmd, data, data_size, multi_block, stop_transmission);
        } else {
            ret = start_command_read_blocks(slot, cmd, data, data_size, stop_transmission);
        }
    } else {
        ret = start_command_default(slot, flags, cmd);
    }

cleanup:
    if (bus_acquired) {
        esp_err_t cs_ret = cs_high(slot);
        if (cs_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to deassert SD CS via CH422G (%s)", esp_err_to_name(cs_ret));
            ret = (ret == ESP_OK) ? cs_ret : ret;
        }

        release_bus(slot);
        spi_device_release_bus(slot->spi_handle);
    }

    if (ret == ESP_OK) {
        if (cmd_index == SD_CRC_ON_OFF) {
            slot->data_crc_enabled = (uint8_t) cmd_arg;
            ESP_LOGD(TAG, "data CRC set=%d", slot->data_crc_enabled);
        }
    } else {
        ESP_LOGD(TAG, "%s: cmd=%d error=0x%x", __func__, cmd_index, ret);
    }

    return ret;
}

static esp_err_t start_command_default(slot_info_t *slot, int flags, sdspi_hw_cmd_t *cmd)
{
    size_t cmd_size = SDSPI_CMD_R1_SIZE;
    if ((flags & SDSPI_CMD_FLAG_RSP_R1) || (flags & SDSPI_CMD_FLAG_NORSP)) {
        cmd_size = SDSPI_CMD_R1_SIZE;
    } else if (flags & SDSPI_CMD_FLAG_RSP_R2) {
        cmd_size = SDSPI_CMD_R2_SIZE;
    } else if (flags & SDSPI_CMD_FLAG_RSP_R3) {
        cmd_size = SDSPI_CMD_R3_SIZE;
    } else if (flags & SDSPI_CMD_FLAG_RSP_R4) {
        cmd_size = SDSPI_CMD_R4_SIZE;
    } else if (flags & SDSPI_CMD_FLAG_RSP_R5) {
        cmd_size = SDSPI_CMD_R5_SIZE;
    } else if (flags & SDSPI_CMD_FLAG_RSP_R7) {
        cmd_size = SDSPI_CMD_R7_SIZE;
    }

    cmd_size += (SDSPI_NCR_MAX_SIZE - SDSPI_NCR_MIN_SIZE);

    spi_transaction_t t = {
        .flags = 0,
        .length = cmd_size * 8,
        .tx_buffer = cmd,
        .rx_buffer = cmd,
    };

    esp_err_t ret = spi_device_polling_transmit(slot->spi_handle, &t);
    if (cmd->cmd_index == MMC_STOP_TRANSMISSION) {
        cmd->r1 = 0xff;
    }
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "%s: spi_device_polling_transmit returned 0x%x", __func__, ret);
        return ret;
    }

    if (flags & SDSPI_CMD_FLAG_NORSP) {
        // No (correct) response expected from the card, skip polling loop
        cmd->r1 = 0x00;
        return ESP_OK;
    }

    ret = shift_cmd_response(cmd, (int)cmd_size);
    if (ret != ESP_OK) return ESP_ERR_TIMEOUT;

    return ESP_OK;
}

static esp_err_t poll_busy(slot_info_t *slot, int timeout_ms, bool polling)
{
    uint8_t t_rx;
    spi_transaction_t t = {
        .tx_buffer = &t_rx,
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 8,
    };

    int64_t t_end = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    int nonzero_count = 0;
    int loop_count = 0;

    do {
        t_rx = SDSPI_MOSI_IDLE_VAL;
        t.rx_data[0] = 0;
        esp_err_t ret = polling ? spi_device_polling_transmit(slot->spi_handle, &t)
                                : spi_device_transmit(slot->spi_handle, &t);
        if (ret != ESP_OK) {
            return ret;
        }
        if (t.rx_data[0] != 0) {
            if (++nonzero_count == 2) {
                return ESP_OK;
            }
        }
        if (++loop_count % 8 == 0) {
            taskYIELD();
        }
    } while (esp_timer_get_time() < t_end);

    ESP_LOGD(TAG, "%s: timeout", __func__);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t poll_data_token(slot_info_t *slot, uint8_t *extra_ptr, size_t *extra_size, int timeout_ms)
{
    uint8_t t_rx[8];
    spi_transaction_t t = {
        .tx_buffer = &t_rx,
        .rx_buffer = &t_rx,
        .length = sizeof(t_rx) * 8,
    };

    int64_t t_end = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    int loop_count = 0;

    do {
        memset(t_rx, SDSPI_MOSI_IDLE_VAL, sizeof(t_rx));
        esp_err_t ret = spi_device_polling_transmit(slot->spi_handle, &t);
        if (ret != ESP_OK) {
            return ret;
        }

        for (size_t byte_idx = 0; byte_idx < sizeof(t_rx); byte_idx++) {
            uint8_t rd_data = t_rx[byte_idx];
            if (rd_data == TOKEN_BLOCK_START) {
                memcpy(extra_ptr, t_rx + byte_idx + 1, sizeof(t_rx) - byte_idx - 1);
                *extra_size = sizeof(t_rx) - byte_idx - 1;
                return ESP_OK;
            }
            if (rd_data != 0xff && rd_data != 0) {
                ESP_LOGD(TAG, "%s: received 0x%02x while waiting for data", __func__, rd_data);
                return ESP_ERR_INVALID_RESPONSE;
            }
        }
        if (++loop_count % 8 == 0) {
            taskYIELD();
        }
    } while (esp_timer_get_time() < t_end);

    ESP_LOGD(TAG, "%s: timeout", __func__);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t shift_cmd_response(sdspi_hw_cmd_t* cmd, int sent_bytes)
{
    uint8_t* pr1 = &cmd->r1;
    int ncr_cnt = 1;

    while (true) {
        if ((*pr1 & SD_SPI_R1_NO_RESPONSE) == 0) break;
        pr1++;
        if (++ncr_cnt > 8) return ESP_ERR_NOT_FOUND;
    }

    int copy_bytes = sent_bytes - SDSPI_CMD_SIZE - ncr_cnt;
    if (copy_bytes > 0) {
        memcpy(&cmd->r1, pr1, copy_bytes);
    }

    return ESP_OK;
}

static esp_err_t start_command_read_blocks(slot_info_t *slot, sdspi_hw_cmd_t *cmd,
        uint8_t *data, uint32_t rx_length, bool need_stop_command)
{
    spi_transaction_t t_command = {
        .length = (SDSPI_CMD_R1_SIZE + SDSPI_RESPONSE_MAX_DELAY) * 8,
        .tx_buffer = cmd,
        .rx_buffer = cmd,
    };
    esp_err_t ret = spi_device_polling_transmit(slot->spi_handle, &t_command);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t* cmd_u8 = (uint8_t*) cmd;
    size_t pre_scan_data_size = SDSPI_RESPONSE_MAX_DELAY;
    uint8_t* pre_scan_data_ptr = cmd_u8 + SDSPI_CMD_R1_SIZE;

    while ((cmd->r1 & SD_SPI_R1_NO_RESPONSE) != 0 && pre_scan_data_size > 0) {
        cmd->r1 = *pre_scan_data_ptr;
        ++pre_scan_data_ptr;
        --pre_scan_data_size;
    }
    if (cmd->r1 & SD_SPI_R1_NO_RESPONSE) {
        ESP_LOGD(TAG, "no response token found");
        return ESP_ERR_TIMEOUT;
    }

    while (rx_length > 0) {
        size_t extra_data_size = 0;
        const uint8_t* extra_data_ptr = NULL;
        bool need_poll = true;

        for (size_t i = 0; i < pre_scan_data_size; ++i) {
            if (pre_scan_data_ptr[i] == TOKEN_BLOCK_START) {
                extra_data_size = pre_scan_data_size - i - 1;
                extra_data_ptr = pre_scan_data_ptr + i + 1;
                need_poll = false;
                break;
            }
        }

        if (need_poll) {
            ret = poll_data_token(slot, cmd_u8 + SDSPI_CMD_R1_SIZE, &extra_data_size, cmd->timeout_ms);
            if (ret != ESP_OK) {
                return ret;
            }
            if (extra_data_size) {
                extra_data_ptr = cmd_u8 + SDSPI_CMD_R1_SIZE;
            }
        }

        size_t will_receive = MIN(rx_length, SDSPI_MAX_DATA_LEN) - extra_data_size;
        uint8_t* rx_data;
        ret = get_block_buf(slot, &rx_data);
        if (ret != ESP_OK) {
            return ret;
        }

        const size_t receive_extra_bytes = (rx_length > SDSPI_MAX_DATA_LEN) ? 4 : 2;
        memset(rx_data, 0xff, will_receive + receive_extra_bytes);

        spi_transaction_t t_data = {
            .length = (will_receive + receive_extra_bytes) * 8,
            .rx_buffer = rx_data,
            .tx_buffer = rx_data
        };

        ret = spi_device_transmit(slot->spi_handle, &t_data);
        if (ret != ESP_OK) {
            return ret;
        }

        uint16_t crc = UINT16_MAX;
        memcpy(&crc, rx_data + will_receive, sizeof(crc));

        pre_scan_data_size = receive_extra_bytes - sizeof(crc);
        pre_scan_data_ptr = rx_data + will_receive + sizeof(crc);

        memcpy(data + extra_data_size, rx_data, will_receive);
        if (extra_data_size) {
            memcpy(data, extra_data_ptr, extra_data_size);
        }

        if (slot->data_crc_enabled) {
            uint16_t crc_of_data = sdspi_crc16(data, will_receive + extra_data_size);
            if (crc_of_data != crc) {
                ESP_LOGE(TAG, "data CRC failed, got=0x%04x expected=0x%04x", crc_of_data, crc);
                ESP_LOG_BUFFER_HEX(TAG, data, 16);
                return ESP_ERR_INVALID_CRC;
            }
        }

        data += will_receive + extra_data_size;
        rx_length -= will_receive + extra_data_size;
    }

    if (need_stop_command) {
        sdspi_hw_cmd_t stop_cmd;
        make_hw_cmd_ch422g(MMC_STOP_TRANSMISSION, 0, cmd->timeout_ms, &stop_cmd);

        ret = start_command_default(slot, SDSPI_CMD_FLAG_RSP_R1, &stop_cmd);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = poll_busy(slot, cmd->timeout_ms, use_polling);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t start_command_write_blocks(slot_info_t *slot, sdspi_hw_cmd_t *cmd,
        const uint8_t *data, uint32_t tx_length, bool multi_block, bool stop_trans)
{
    if (card_write_protected(slot)) {
        ESP_LOGW(TAG, "%s: card write protected", __func__);
        return ESP_ERR_INVALID_STATE;
    }

    const int send_bytes = SDSPI_CMD_R5_SIZE + SDSPI_NCR_MAX_SIZE - SDSPI_NCR_MIN_SIZE;

    spi_transaction_t t_command = {
        .length = send_bytes * 8,
        .tx_buffer = cmd,
        .rx_buffer = cmd,
    };
    esp_err_t ret = spi_device_polling_transmit(slot->spi_handle, &t_command);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = shift_cmd_response(cmd, send_bytes);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "%s: check_cmd_response returned 0x%x", __func__, ret);
        return ret;
    }

    uint8_t start_token = multi_block ? TOKEN_BLOCK_START_WRITE_MULTI : TOKEN_BLOCK_START;

    while (tx_length > 0) {
        spi_transaction_t t_start_token = {
            .length = sizeof(start_token) * 8,
            .tx_buffer = &start_token
        };
        ret = spi_device_polling_transmit(slot->spi_handle, &t_start_token);
        if (ret != ESP_OK) {
            return ret;
        }

        size_t will_send = MIN(tx_length, SDSPI_MAX_DATA_LEN);
        const uint8_t* tx_data = data;

        if (!esp_ptr_in_dram(tx_data)) {
            uint8_t* tmp;
            ret = get_block_buf(slot, &tmp);
            if (ret != ESP_OK) {
                return ret;
            }
            memcpy(tmp, tx_data, will_send);
            tx_data = tmp;
        }

        spi_transaction_t t_data = {
            .length = will_send * 8,
            .tx_buffer = tx_data,
        };
        ret = spi_device_transmit(slot->spi_handle, &t_data);
        if (ret != ESP_OK) {
            return ret;
        }

        uint16_t crc = sdspi_crc16((const uint8_t*)data, will_send);
        const int size_crc_response = (int)(sizeof(crc) + 1);

        spi_transaction_t t_crc_rsp = {
            .length = size_crc_response * 8,
            .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        };
        memset(t_crc_rsp.tx_data, 0xff, 4);
        memcpy(t_crc_rsp.tx_data, &crc, sizeof(crc));

        ret = spi_device_polling_transmit(slot->spi_handle, &t_crc_rsp);
        if (ret != ESP_OK) {
            return ret;
        }

        uint8_t data_rsp = t_crc_rsp.rx_data[2];
        if (!SD_SPI_DATA_RSP_VALID(data_rsp)) return ESP_ERR_INVALID_RESPONSE;

        switch (SD_SPI_DATA_RSP(data_rsp)) {
        case SD_SPI_DATA_ACCEPTED:
            break;
        case SD_SPI_DATA_CRC_ERROR:
            return ESP_ERR_INVALID_CRC;
        case SD_SPI_DATA_WR_ERROR:
            return ESP_FAIL;
        default:
            return ESP_ERR_INVALID_RESPONSE;
        }

        ret = poll_busy(slot, cmd->timeout_ms, no_use_polling);
        if (ret != ESP_OK) {
            return ret;
        }

        tx_length -= will_send;
        data += will_send;
    }

    if (stop_trans) {
        uint8_t stop_token[2] = {
            TOKEN_BLOCK_STOP_WRITE_MULTI,
            SDSPI_MOSI_IDLE_VAL
        };
        spi_transaction_t t_stop_token = {
            .length = sizeof(stop_token) * 8,
            .tx_buffer = &stop_token,
        };
        ret = spi_device_polling_transmit(slot->spi_handle, &t_stop_token);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = poll_busy(slot, cmd->timeout_ms, use_polling);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t sdspi_host_ch422g_do_transaction(int slot, sdmmc_command_t *cmdinfo)
{
    _lock_acquire(&s_lock);

    WORD_ALIGNED_ATTR sdspi_hw_cmd_t hw_cmd;
    make_hw_cmd_ch422g(cmdinfo->opcode, cmdinfo->arg, cmdinfo->timeout_ms, &hw_cmd);

    int flags = 0;
    if (SCF_CMD(cmdinfo->flags) == SCF_CMD_ADTC) {
        flags = SDSPI_CMD_FLAG_DATA | SDSPI_CMD_FLAG_WRITE;
    } else if (SCF_CMD(cmdinfo->flags) == (SCF_CMD_ADTC | SCF_CMD_READ)) {
        flags = SDSPI_CMD_FLAG_DATA;
    }

    if (cmdinfo->datalen > SDSPI_MAX_DATA_LEN) {
        flags |= SDSPI_CMD_FLAG_MULTI_BLK;
    }

    if (!s_app_cmd && cmdinfo->opcode == SD_SEND_IF_COND) {
        flags |= SDSPI_CMD_FLAG_RSP_R7;
    } else if (!s_app_cmd && cmdinfo->opcode == MMC_SEND_STATUS) {
        flags |= SDSPI_CMD_FLAG_RSP_R2;
    } else if (!s_app_cmd && cmdinfo->opcode == SD_READ_OCR) {
        flags |= SDSPI_CMD_FLAG_RSP_R3;
    } else if (s_app_cmd && cmdinfo->opcode == SD_APP_SD_STATUS) {
        flags |= SDSPI_CMD_FLAG_RSP_R2;
    } else if (!s_app_cmd && cmdinfo->opcode == MMC_GO_IDLE_STATE && !(cmdinfo->flags & SCF_RSP_R1)) {
        flags |= SDSPI_CMD_FLAG_NORSP;
    } else if (!s_app_cmd && cmdinfo->opcode == SD_IO_SEND_OP_COND) {
        flags |= SDSPI_CMD_FLAG_RSP_R4;
    } else if (!s_app_cmd && cmdinfo->opcode == SD_IO_RW_DIRECT) {
        flags |= SDSPI_CMD_FLAG_RSP_R5;
    } else if (!s_app_cmd && cmdinfo->opcode == SD_IO_RW_EXTENDED) {
        flags |= SDSPI_CMD_FLAG_RSP_R5 | SDSPI_CMD_FLAG_DATA;
        if (cmdinfo->arg & SD_ARG_CMD53_WRITE) flags |= SDSPI_CMD_FLAG_WRITE;
        if (cmdinfo->arg & SD_ARG_CMD53_BLOCK_MODE) flags |= SDSPI_CMD_FLAG_MULTI_BLK;
    } else {
        flags |= SDSPI_CMD_FLAG_RSP_R1;
    }

    esp_err_t ret = sdspi_host_ch422g_start_command(slot, &hw_cmd, cmdinfo->data, cmdinfo->datalen, flags);

    if (ret == ESP_OK) {
        if (flags & SDSPI_CMD_FLAG_RSP_R1) {
            cmdinfo->response[0] = hw_cmd.r1;
            r1_response_to_err(hw_cmd.r1, cmdinfo->opcode, &ret);
        } else if (flags & SDSPI_CMD_FLAG_RSP_R2) {
            cmdinfo->response[0] = (((uint32_t)hw_cmd.r1) << 8) | (hw_cmd.response[0] >> 24);
        } else if (flags & (SDSPI_CMD_FLAG_RSP_R3 | SDSPI_CMD_FLAG_RSP_R7)) {
            r1_response_to_err(hw_cmd.r1, cmdinfo->opcode, &ret);
            cmdinfo->response[0] = __builtin_bswap32(hw_cmd.response[0]);
        } else if (flags & SDSPI_CMD_FLAG_RSP_R4) {
            r1_sdio_response_to_err(hw_cmd.r1, cmdinfo->opcode, &ret);
            cmdinfo->response[0] = __builtin_bswap32(hw_cmd.response[0]);
        } else if (flags & SDSPI_CMD_FLAG_RSP_R5) {
            r1_sdio_response_to_err(hw_cmd.r1, cmdinfo->opcode, &ret);
            cmdinfo->response[0] = hw_cmd.response[0];
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "CMD%d failed: ret=0x%x, r1=0x%02x", cmdinfo->opcode, ret, hw_cmd.r1);
    }

    s_app_cmd = (ret == ESP_OK && cmdinfo->opcode == MMC_APP_CMD);
    _lock_release(&s_lock);
    return ret;
}

esp_err_t sdspi_host_ch422g_io_int_enable(sdspi_dev_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t sdspi_host_ch422g_io_int_wait(sdspi_dev_handle_t handle, TickType_t timeout_ticks)
{
    (void)handle;
    (void)timeout_ticks;
    return ESP_OK;
}

// Deprecated API, kept for compatibility with older code.
esp_err_t sdspi_host_ch422g_init_slot(int slot, const sdspi_slot_config_t* slot_config)
{
    esp_err_t ret = ESP_OK;
    if (get_slot_info(slot) != NULL) {
        ESP_LOGE(TAG, "Bus already initialized. Call `sdspi_host_init_dev` to attach an sdspi device.");
        return ESP_ERR_INVALID_STATE;
    }

    spi_host_device_t host_id = slot;

    spi_bus_config_t buscfg = {
        .miso_io_num = slot_config->gpio_miso,
        .mosi_io_num = slot_config->gpio_mosi,
        .sclk_io_num = slot_config->gpio_sck,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC
    };

    ret = spi_bus_initialize(host_id, &buscfg, slot_config->dma_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed with rc=0x%x", ret);
        return ret;
    }

    sdspi_dev_handle_t sdspi_handle;
    sdspi_device_config_t dev_config = {
        .host_id = host_id,
        .gpio_cs = slot_config->gpio_cs,
        .gpio_cd = slot_config->gpio_cd,
        .gpio_wp = slot_config->gpio_wp,
        .gpio_int = slot_config->gpio_int,
    };

    ret = sdspi_host_ch422g_init_device(&dev_config, &sdspi_handle);
    if (ret != ESP_OK) {
        spi_bus_free(slot);
        return ret;
    }

    if (sdspi_handle != (int)host_id) {
        ESP_LOGE(TAG, "Deprecated sdspi_host_init_slot should be called before other devices on the bus.");
        (void)sdspi_host_ch422g_remove_device(sdspi_handle);
        spi_bus_free(slot);
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

sdmmc_host_t sdspi_host_ch422g_default(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.init = &sdspi_host_ch422g_init;
    host.set_card_clk = &sdspi_host_ch422g_set_card_clk;
    host.do_transaction = &sdspi_host_ch422g_do_transaction;
#if defined(SDMMC_HOST_FLAG_DEINIT_ARG)
    host.deinit_p = &sdspi_host_ch422g_remove_device;
#else
    host.deinit = &sdspi_host_ch422g_remove_device;
#endif
    host.io_int_enable = &sdspi_host_ch422g_io_int_enable;
    host.io_int_wait = &sdspi_host_ch422g_io_int_wait;
    return host;
}
