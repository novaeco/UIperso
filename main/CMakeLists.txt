idf_component_register(
    SRCS
        "app_main.c"
        "ui.c"
        "ui_events.c"
        "ui_helpers.c"
        "app_ui.c"
        "app_hw.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer esp_wifi driver esp_lcd nvs_flash
)
