idf_component_register(
    SRCS
        "app_main.c"
        "ui_init.c"
        "app_hw.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer esp_wifi nvs_flash driver
)

# TODO: ajouter fichiers générés SquareLine (ui.c, ui_helpers.c, ui_events.c) dans SRCS quand disponibles
