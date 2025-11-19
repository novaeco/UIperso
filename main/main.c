#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_efuse.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "ESP32-S3 UI bootstrap phase 1 started");
    ESP_LOGI(TAG, "Chip revision: %d", esp_efuse_get_chip_ver());
    ESP_LOGI(TAG, "Free heap: %u bytes", (unsigned int)esp_get_free_heap_size());

    while (true) {
        ESP_LOGD(TAG, "Heartbeat: system alive");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
