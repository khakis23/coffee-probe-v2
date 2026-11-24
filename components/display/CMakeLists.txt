# The esp_driver_i2c component exists in both IDF v5.3 and v6.0.
idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES board esp_driver_i2c esp_timer
)
