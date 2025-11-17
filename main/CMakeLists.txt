idf_component_register(
    SRCS
        "app_main.c"
        "ui_init.c"
        "app_hw.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer esp_wifi esp_event esp_netif nvs_flash driver esp_lcd esp_vfs_fat freertos
)
