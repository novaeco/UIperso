idf_component_register(SRCS "sdspi_ch422g.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver sdmmc esp_driver_spi esp_driver_sdmmc ch422g
                       PRIV_REQUIRES esp_driver_sdspi)
