idf_component_register(
    SRCS "health.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES esp_driver_tsens esp_hw_support esp_system esp_timer
)
