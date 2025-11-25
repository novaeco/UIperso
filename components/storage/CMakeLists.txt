idf_component_register(
    SRCS "sdcard.c"
    INCLUDE_DIRS "."
    REQUIRES driver fatfs ch422g sdspi_ch422g
    PRIV_REQUIRES log
)
