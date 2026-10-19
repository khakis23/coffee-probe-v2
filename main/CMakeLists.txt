# main: wiring only (init order, task creation). Logic lives in components/.
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES board bringup esp_timer spi_flash
)
