idf_component_register(
    SRCS "net.cpp" "modbus_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core esp_common
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_timer board lwip
)
