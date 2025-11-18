#include "storage.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "storage";

esp_err_t storage_init(void)
{
    // Placeholder for extended parameter sets; NVS already initialized in main
    ESP_LOGI(TAG, "Storage initialized (NVS ready)");
    return ESP_OK;
}
