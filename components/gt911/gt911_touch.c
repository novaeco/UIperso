#include "gt911_touch.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#define GT911_I2C_PORT                 I2C_NUM_0
#define GT911_I2C_SDA_GPIO             GPIO_NUM_8
#define GT911_I2C_SCL_GPIO             GPIO_NUM_9
#define GT911_I2C_SPEED_HZ             400000
#define GT911_I2C_ADDRESS              0x5D
#define GT911_I2C_TIMEOUT_MS           50

#define GT911_REG_COMMAND              0x8040
#define GT911_REG_PRODUCT_ID           0x8140
#define GT911_REG_VENDOR_ID            0x814A
#define GT911_REG_STATUS               0x814E
#define GT911_REG_FIRST_POINT          0x8150

#define GT911_POINT_STRUCT_SIZE        8
#define GT911_MAX_TOUCH_POINTS         5

#define GT911_INT_GPIO                 GPIO_NUM_4   // IRQ routed directly to ESP32-S3 on Waveshare board
#define GT911_RST_GPIO                 GPIO_NUM_NC  // RST routed via IO extension (not controlled here)

static const char *TAG = "GT911";

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t size;
    bool valid;
} gt911_point_t;

static lv_indev_t *s_indev = NULL;
static gt911_point_t s_last_point;
static bool s_initialized = false;

static esp_err_t gt911_bus_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GT911_I2C_SDA_GPIO,
        .scl_io_num = GT911_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = GT911_I2C_SPEED_HZ,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(GT911_I2C_PORT, &cfg);
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE))
    {
        ESP_LOGE(TAG, "i2c_param_config failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(GT911_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE))
    {
        ESP_LOGE(TAG, "i2c_driver_install failed (%s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

static esp_err_t gt911_write_u8(uint16_t reg, uint8_t value)
{
    uint8_t payload[3] = {reg & 0xFF, (reg >> 8) & 0xFF, value};
    return i2c_master_write_to_device(
        GT911_I2C_PORT,
        GT911_I2C_ADDRESS,
        payload,
        sizeof(payload),
        pdMS_TO_TICKS(GT911_I2C_TIMEOUT_MS));
}

static esp_err_t gt911_read(uint16_t reg, uint8_t *data, size_t length)
{
    uint8_t reg_buf[2] = {reg & 0xFF, (reg >> 8) & 0xFF};
    return i2c_master_write_read_device(
        GT911_I2C_PORT,
        GT911_I2C_ADDRESS,
        reg_buf,
        sizeof(reg_buf),
        data,
        length,
        pdMS_TO_TICKS(GT911_I2C_TIMEOUT_MS));
}

static void gt911_clear_status(void)
{
    const uint8_t zero = 0;
    if (gt911_write_u8(GT911_REG_STATUS, zero) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to clear touch status");
    }
}

static bool gt911_read_primary_point(gt911_point_t *point)
{
    uint8_t status = 0;
    if (gt911_read(GT911_REG_STATUS, &status, sizeof(status)) != ESP_OK)
    {
        return false;
    }

    if ((status & 0x80U) == 0)
    {
        return false;
    }

    const uint8_t touches = status & 0x0FU;
    if (touches == 0)
    {
        gt911_clear_status();
        return false;
    }

    uint8_t buf[GT911_POINT_STRUCT_SIZE];
    if (gt911_read(GT911_REG_FIRST_POINT, buf, sizeof(buf)) != ESP_OK)
    {
        gt911_clear_status();
        return false;
    }

    point->x = ((uint16_t)buf[1] << 8) | buf[0];
    point->y = ((uint16_t)buf[3] << 8) | buf[2];
    point->size = ((uint16_t)buf[5] << 8) | buf[4];
    point->valid = true;

    gt911_clear_status();
    return true;
}

static void gt911_hw_reset(void)
{
    if (GT911_RST_GPIO == GPIO_NUM_NC)
    {
        ESP_LOGW(TAG, "GT911 reset line not controlled (IO extension not implemented)");
        return;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GT911_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    gpio_set_level(GT911_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(GT911_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(60));
}

static void gt911_configure_int_pin(void)
{
    if (GT911_INT_GPIO == GPIO_NUM_NC)
    {
        return;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GT911_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static bool gt911_poll(gt911_point_t *point)
{
    memset(point, 0, sizeof(*point));
    return gt911_read_primary_point(point);
}

static void gt911_log_identity(void)
{
    uint8_t product_id[4] = {0};
    uint8_t vendor = 0;
    if (gt911_read(GT911_REG_PRODUCT_ID, product_id, sizeof(product_id)) == ESP_OK &&
        gt911_read(GT911_REG_VENDOR_ID, &vendor, sizeof(vendor)) == ESP_OK)
    {
        char id_str[5] = {0};
        memcpy(id_str, product_id, sizeof(product_id));
        ESP_LOGI(TAG, "GT911 ID=%s vendor=0x%02X", id_str, vendor);
    }
    else
    {
        ESP_LOGW(TAG, "Unable to read GT911 product ID");
    }
}

static void gt911_lvgl_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    gt911_point_t point = {0};
    const bool touched = gt911_poll(&point);

    if (touched && point.valid)
    {
        s_last_point = point;
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->point.x = s_last_point.x;
        data->point.y = s_last_point.y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->continue_reading = false;
}

void gt911_init(void)
{
    if (s_initialized)
    {
        return;
    }

    if (gt911_bus_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C bus for GT911");
        return;
    }

    gt911_hw_reset();
    gt911_configure_int_pin();
    gt911_log_identity();

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, gt911_lvgl_read);
    lv_indev_set_display(s_indev, lv_display_get_default());
    if (s_indev == NULL)
    {
        ESP_LOGE(TAG, "Failed to register LVGL input device");
        return;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "GT911 touch initialized");
}

lv_indev_t *gt911_get_input_device(void)
{
    return s_indev;
}

