idf_component_register(SRCS "main.c"
                            "dashboard.c"
                            "logs_panel.c"
                            "lv_theme_custom.c"
                            "system_panel.c"
                            "ui_smoke.c"
                            "ui_manager.c"
                       INCLUDE_DIRS "."
                       REQUIRES bus ch422g display gt911 lvgl power sdspi_ch422g storage)
