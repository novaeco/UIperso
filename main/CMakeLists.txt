file(GLOB MAIN_SRCS "*.c")

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        bus
        ch422g
        display
        gt911
        storage
        net
)
