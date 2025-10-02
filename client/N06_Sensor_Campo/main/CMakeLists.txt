idf_component_register(
    SRCS "hello_world_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_spi sdmmc fatfs vfs
)