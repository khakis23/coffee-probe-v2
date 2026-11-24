idf_component_register(
    SRCS "thermocouple.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES board esp_driver_spi esp_system esp_timer
)
