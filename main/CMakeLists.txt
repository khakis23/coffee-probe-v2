# main: wiring only (init order, task creation). Logic lives in components/.
idf_component_register(
    SRCS "app_main.cpp" "tasks.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES board bringup display esp_app_format esp_system esp_timer spi_flash health net nvs_flash state storage thermocouple
)
