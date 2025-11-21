#include "i2c_bus_shared.h"

#include "esp_check.h"
#include "esp_log.h"

#define SHARED_I2C_PORT         I2C_NUM_0
#define SHARED_I2C_SDA_GPIO     GPIO_NUM_8
#define SHARED_I2C_SCL_GPIO     GPIO_NUM_9
#define SHARED_I2C_FREQ_HZ      100000
#define SHARED_I2C_TIMEOUT_MS   50

static const char *TAG = "i2c_bus_shared";
static i2c_master_bus_handle_t s_bus = NULL;

esp_err_t i2c_bus_shared_init(void)
{
    if (s_bus != NULL)
    {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = SHARED_I2C_PORT,
        .sda_io_num = SHARED_I2C_SDA_GPIO,
        .scl_io_num = SHARED_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
            .allow_pd = 0,
        },
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c_new_master_bus failed");

    ESP_LOGI(TAG, "Shared I2C initialized on port %d (SDA=%d SCL=%d %u Hz)",
             SHARED_I2C_PORT,
             SHARED_I2C_SDA_GPIO,
             SHARED_I2C_SCL_GPIO,
             SHARED_I2C_FREQ_HZ);

    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_shared_handle(void)
{
    return s_bus;
}

int i2c_bus_shared_timeout_ms(void)
{
    return SHARED_I2C_TIMEOUT_MS;
}

uint32_t i2c_bus_shared_default_speed_hz(void)
{
    return SHARED_I2C_FREQ_HZ;
}

esp_err_t i2c_bus_shared_add_device(uint16_t address,
                                    uint32_t scl_speed_hz,
                                    i2c_master_dev_handle_t *ret_handle)
{
    if (s_bus == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (ret_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = scl_speed_hz,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };

    return i2c_master_bus_add_device(s_bus, &dev_cfg, ret_handle);
}

