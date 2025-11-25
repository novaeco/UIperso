idf_component_register(
    SRCS
        "sdspi_ch422g.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        sdmmc
        esp_driver_spi
        esp_driver_sdmmc
        ch422g
        bus
    PRIV_REQUIRES
        log
        esp_driver_sdspi
)

if(DEFINED ENV{IDF_PATH})
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "$ENV{IDF_PATH}/components/esp_driver_sdspi/src"
    )
else()
    message(FATAL_ERROR "IDF_PATH is not defined in the environment, cannot add include path for sdspi_private.h")
endif()
