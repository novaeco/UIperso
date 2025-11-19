#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ESP32-S3 UI bootstrap phase 1 started");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip model %d, %d core(s), revision %d", chip_info.model, chip_info.cores, chip_info.revision);

    while (true)
    {
        ESP_LOGI(TAG, "System heartbeat, free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
