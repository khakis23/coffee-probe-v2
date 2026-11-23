idf_component_register(
    SRCS "state_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core freertos
    PRIV_REQUIRES esp_timer log
)
