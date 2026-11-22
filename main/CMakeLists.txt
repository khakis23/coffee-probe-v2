idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bringup esp_timer spi_flash
)
