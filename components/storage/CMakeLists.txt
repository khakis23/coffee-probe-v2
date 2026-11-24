idf_component_register(
    SRCS "storage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES nvs_flash
)
