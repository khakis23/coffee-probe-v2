# board: the only place GPIO numbers appear. Header-mostly; the .cpp just gives the
# component something to compile so IDF treats it as a normal library.
idf_component_register(
    SRCS "board.cpp"
    INCLUDE_DIRS "include"
    # Public (REQUIRES, not PRIV_REQUIRES): board.hpp includes driver/gpio.h for gpio_num_t,
    # so every component that includes board.hpp needs that include path too.
    REQUIRES esp_driver_gpio
)
