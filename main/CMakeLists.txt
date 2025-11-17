idf_component_register(
    SRCS
        "app_main.c"
        "hal_display.c"
        "hal_touch.c"
        "hal_sdcard.c"
        "hal_can.c"
        "hal_rs485.c"
        "hal_ioexp_ch422g.c"
        "ui/ui_init.c"
        "ui/ui_screens.c"
        "ui/ui_events.c"
        "ui/ui_theme.c"
    INCLUDE_DIRS
        "."
        "ui"
    REQUIRES
        lvgl
        esp_lvgl_port
)

set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD 11)
