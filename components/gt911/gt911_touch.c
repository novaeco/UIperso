#include "gt911_touch.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ch422g.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "i2c_bus_shared.h"

#define GT911_I2C_ADDRESS              0x5D
#define GT911_I2C_TIMEOUT_TICKS        i2c_bus_shared_timeout_ticks()
#define GT911_I2C_RETRIES              3
#define GT911_I2C_RETRY_DELAY_MS       3

#define GT911_REG_COMMAND              0x8040
#define GT911_REG_CONFIG               0x8047
#define GT911_REG_CONFIG_CHECKSUM      0x80FF
#define GT911_REG_CONFIG_FRESH         0x8100
#define GT911_REG_PRODUCT_ID           0x8140
#define GT911_REG_VENDOR_ID            0x814A
#define GT911_REG_STATUS               0x814E
#define GT911_REG_FIRST_POINT          0x8150

#define GT911_POINT_STRUCT_SIZE        8
#define GT911_MAX_TOUCH_POINTS         5
#define GT911_CONFIG_DATA_LENGTH       184

#define GT911_RESOLUTION_X             1024
#define GT911_RESOLUTION_Y             600
#define GT911_REPORT_RATE_HZ           60

#define GT911_SWAP_AXES                0
#define GT911_INVERT_X                 0
#define GT911_INVERT_Y                 0

#define GT911_MODULE_SWITCH_X2Y_BIT    (1U << 3)
#define GT911_MODULE_SWITCH_MIRROR_Y   (1U << 2)
#define GT911_MODULE_SWITCH_MIRROR_X   (1U << 1)

#define GT911_INT_GPIO                 GPIO_NUM_4   // IRQ routed directly to ESP32-S3 on Waveshare board

#define GT911_LOG_TOUCH_EVENTS         0

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

static inline i2c_port_t gt911_i2c_port(void);

static esp_err_t gt911_retry_write_to_device(const uint8_t *payload, size_t length)
{
    esp_err_t err = ESP_FAIL;

    for (int attempt = 1; attempt <= GT911_I2C_RETRIES; ++attempt)
    {
        err = i2c_master_write_to_device(
            gt911_i2c_port(),
            GT911_I2C_ADDRESS,
            payload,
            length,
            GT911_I2C_TIMEOUT_TICKS);

        if (err == ESP_OK)
        {
            break;
        }

        ESP_LOGW(TAG, "GT911 write attempt %d/%d failed: %s", attempt, GT911_I2C_RETRIES, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(GT911_I2C_RETRY_DELAY_MS));
    }

    return err;
}

static esp_err_t gt911_retry_write_read(const uint8_t *write_buf, size_t write_len, uint8_t *read_buf, size_t read_len)
{
    esp_err_t err = ESP_FAIL;

    for (int attempt = 1; attempt <= GT911_I2C_RETRIES; ++attempt)
    {
        err = i2c_master_write_read_device(
            gt911_i2c_port(),
            GT911_I2C_ADDRESS,
            write_buf,
            write_len,
            read_buf,
            read_len,
            GT911_I2C_TIMEOUT_TICKS);

        if (err == ESP_OK)
        {
            break;
        }

        ESP_LOGW(TAG, "GT911 write-read attempt %d/%d failed: %s", attempt, GT911_I2C_RETRIES, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(GT911_I2C_RETRY_DELAY_MS));
    }

    return err;
}

static inline i2c_port_t gt911_i2c_port(void)
{
    return i2c_bus_shared_port();
}

static esp_err_t gt911_bus_init(void)
{
    ESP_RETURN_ON_ERROR(i2c_bus_shared_init(), TAG, "Shared I2C init failed");

    if (!ch422g_is_available())
    {
        ESP_LOGW(TAG, "IO extension not available; GT911 may not respond");
    }

    return ESP_OK;
}

static esp_err_t gt911_write_u8(uint16_t reg, uint8_t value)
{
    uint8_t payload[3] = {(uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF), value};
    return gt911_retry_write_to_device(payload, sizeof(payload));
}

static esp_err_t gt911_write(uint16_t reg, const uint8_t *data, size_t length)
{
    uint8_t header[2] = {(uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF)};
    const size_t payload_len = length + sizeof(header);
    uint8_t *payload = heap_caps_malloc(payload_len, MALLOC_CAP_INTERNAL);
    if (payload == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    memcpy(payload, header, sizeof(header));
    memcpy(payload + sizeof(header), data, length);

    const esp_err_t err = gt911_retry_write_to_device(payload, payload_len);

    heap_caps_free(payload);
    return err;
}

static esp_err_t gt911_read(uint16_t reg, uint8_t *data, size_t length)
{
    uint8_t reg_buf[2] = {(uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF)};
    return gt911_retry_write_read(reg_buf, sizeof(reg_buf), data, length);
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

static esp_err_t gt911_hw_reset(void)
{
    esp_err_t err = ch422g_set_touch_reset(true);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Unable to drive GT911 reset via IO extension (%s)", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(15));
    err = ch422g_set_touch_reset(false);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to release GT911 reset (%s)", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(60));
    return ESP_OK;
}

static esp_err_t gt911_software_reset(void)
{
    const esp_err_t err = gt911_write_u8(GT911_REG_COMMAND, 0x02);
    if (err == ESP_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    return err;
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

static bool gt911_log_identity(void)
{
    uint8_t product_id[4] = {0};
    uint8_t vendor = 0;
    esp_err_t err = ESP_FAIL;
    for (int attempt = 1; attempt <= GT911_I2C_RETRIES; ++attempt)
    {
        const esp_err_t id_err = gt911_read(GT911_REG_PRODUCT_ID, product_id, sizeof(product_id));
        const esp_err_t vendor_err = gt911_read(GT911_REG_VENDOR_ID, &vendor, sizeof(vendor));
        err = (id_err == ESP_OK && vendor_err == ESP_OK) ? ESP_OK : ESP_FAIL;
        if (err == ESP_OK)
        {
            break;
        }

        ESP_LOGW(TAG, "GT911 ID read attempt %d/%d failed (id=%s vendor=%s)",
                 attempt,
                 GT911_I2C_RETRIES,
                 esp_err_to_name(id_err),
                 esp_err_to_name(vendor_err));
        vTaskDelay(pdMS_TO_TICKS(GT911_I2C_RETRY_DELAY_MS));
    }

    if (err == ESP_OK)
    {
        char id_str[5] = {0};
        memcpy(id_str, product_id, sizeof(product_id));
        ESP_LOGI(TAG, "GT911 ID=%s vendor=0x%02X", id_str, vendor);
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "Unable to read GT911 product ID after %d attempts", GT911_I2C_RETRIES);
    }

    return false;
}

static void gt911_lvgl_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    gt911_point_t point = {0};
    const bool touched = gt911_poll(&point);

    if (touched && point.valid)
    {
        s_last_point = point;
        uint16_t x = point.x;
        uint16_t y = point.y;

        if (GT911_SWAP_AXES)
        {
            const uint16_t tmp = x;
            x = y;
            y = tmp;
        }
        if (GT911_INVERT_X)
        {
            x = (uint16_t)(GT911_RESOLUTION_X - 1U - x);
        }
        if (GT911_INVERT_Y)
        {
            y = (uint16_t)(GT911_RESOLUTION_Y - 1U - y);
        }

#if GT911_LOG_TOUCH_EVENTS
        ESP_LOGI(TAG, "Touch x=%u y=%u size=%u", x, y, point.size);
#endif

        data->point.x = x;
        data->point.y = y;
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

static uint8_t gt911_compute_checksum(const uint8_t *config)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < GT911_CONFIG_DATA_LENGTH; ++i)
    {
        sum = (uint8_t)(sum + config[i]);
    }

    return (uint8_t)((~sum) + 1U);
}

static esp_err_t gt911_update_config(void)
{
    uint8_t config[GT911_CONFIG_DATA_LENGTH] = {0};
    esp_err_t err = gt911_read(GT911_REG_CONFIG, config, sizeof(config));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read GT911 config: %s", esp_err_to_name(err));
        return err;
    }

    const uint8_t config_version = config[0];
    const uint16_t panel_x = ((uint16_t)config[2] << 8) | config[1];
    const uint16_t panel_y = ((uint16_t)config[4] << 8) | config[3];
    ESP_LOGI(TAG, "GT911 config read: version=%u, panel %ux%u", config_version, panel_x, panel_y);

    config[1] = (uint8_t)(GT911_RESOLUTION_X & 0xFF);
    config[2] = (uint8_t)((GT911_RESOLUTION_X >> 8) & 0xFF);
    config[3] = (uint8_t)(GT911_RESOLUTION_Y & 0xFF);
    config[4] = (uint8_t)((GT911_RESOLUTION_Y >> 8) & 0xFF);

    uint8_t module_switch = config[6];
    module_switch &= (uint8_t)~(GT911_MODULE_SWITCH_X2Y_BIT | GT911_MODULE_SWITCH_MIRROR_X | GT911_MODULE_SWITCH_MIRROR_Y);
    if (GT911_SWAP_AXES)
    {
        module_switch |= GT911_MODULE_SWITCH_X2Y_BIT;
    }
    if (GT911_INVERT_X)
    {
        module_switch |= GT911_MODULE_SWITCH_MIRROR_X;
    }
    if (GT911_INVERT_Y)
    {
        module_switch |= GT911_MODULE_SWITCH_MIRROR_Y;
    }
    config[6] = module_switch;

    config[0x0F] = GT911_REPORT_RATE_HZ;

    const uint8_t checksum = gt911_compute_checksum(config);
    uint8_t payload[GT911_CONFIG_DATA_LENGTH + 2];
    memcpy(payload, config, sizeof(config));
    payload[GT911_CONFIG_DATA_LENGTH] = checksum;
    payload[GT911_CONFIG_DATA_LENGTH + 1] = 0x01; // Config_Fresh

    err = gt911_write(GT911_REG_CONFIG, payload, sizeof(payload));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write GT911 config: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "GT911 config written: %ux%u, report %u Hz (swap=%d flipX=%d flipY=%d)",
             GT911_RESOLUTION_X,
             GT911_RESOLUTION_Y,
             GT911_REPORT_RATE_HZ,
             GT911_SWAP_AXES,
             GT911_INVERT_X,
             GT911_INVERT_Y);
    return ESP_OK;
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

    const esp_err_t reset_err = gt911_hw_reset();
    if (reset_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Hardware reset via CH422G failed (%s); attempting GT911 software reset", esp_err_to_name(reset_err));
        const esp_err_t sw_reset_err = gt911_software_reset();
        if (sw_reset_err != ESP_OK)
        {
            ESP_LOGE(TAG, "GT911 software reset also failed (%s)", esp_err_to_name(sw_reset_err));
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    gt911_configure_int_pin();
    const bool identity_ok = gt911_log_identity();
    if (!identity_ok)
    {
        ESP_LOGE(TAG, "GT911 disabled after ID read failures; touch will be unavailable");
        return;
    }

    if (gt911_update_config() != ESP_OK)
    {
        ESP_LOGW(TAG, "GT911 configuration update failed; continuing with defaults (%ux%u swap=%d flipX=%d flipY=%d)",
                 GT911_RESOLUTION_X,
                 GT911_RESOLUTION_Y,
                 GT911_SWAP_AXES,
                 GT911_INVERT_X,
                 GT911_INVERT_Y);
    }
    else
    {
        ESP_LOGI(TAG, "GT911 ready: %ux%u, report %u Hz", GT911_RESOLUTION_X, GT911_RESOLUTION_Y, GT911_REPORT_RATE_HZ);
    }

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

