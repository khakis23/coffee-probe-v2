# Bring-up diagnostics (#2/#3): I2C scan, OLED test pattern, raw thermocouple frames.
# Only runs when CONFIG_PROBE_BRINGUP_DIAG=y (see main.cpp).
# The esp_driver_* components exist in both IDF v5.3 and v6.0.
idf_component_register(
    SRCS "bringup.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board esp_driver_i2c esp_driver_spi esp_driver_gpio
)
