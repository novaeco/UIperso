idf_component_register(
    SRCS "main.c" "ui_main.c" "wifi_manager.c" "bt_manager.c" "storage.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl_port sd rgb_lcd_port touch esp_wifi esp_netif esp_event bt nvs_flash fatfs lwip
)

# Silence format warnings from LVGL until upstream headers are patched
idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
