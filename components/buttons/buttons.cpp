#include "buttons/buttons.hpp"

#include "board/board.hpp"
#include "core/button.hpp"
#include "driver/gpio.h"

namespace buttons {
namespace {

using core::menu::Button;

struct Input {
    gpio_num_t pin;
    Button button;
};

constexpr Input kInputs[kButtons] = {
    {board::kBoard.pins.btn_up, Button::Up},
    {board::kBoard.pins.btn_down, Button::Down},
    {board::kBoard.pins.btn_ok, Button::Ok},
    {board::kBoard.pins.btn_back, Button::Back},
};

// The UI task is the only caller of poll().
core::Debouncer debouncers[kButtons];
// A button already down when polling starts (OK still held after the power-on factory reset)
// is ignored until it has been seen released, so it can't open the menu by itself.
bool armed[kButtons]{};

bool down(gpio_num_t pin) {
    // GPIO_NUM_NC: a board without that button never reports it pressed.
    return pin != GPIO_NUM_NC && gpio_get_level(pin) == 0;
}

}  // namespace

esp_err_t init() {
    uint64_t mask = 0;
    for (const auto& input : kInputs) {
        if (input.pin != GPIO_NUM_NC) {
            mask |= 1ULL << input.pin;
        }
    }
    if (mask == 0) {
        return ESP_OK;
    }
    gpio_config_t config{};
    config.pin_bit_mask = mask;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

int poll(uint32_t now_ms, core::menu::Press (&out)[kButtons]) {
    int count = 0;
    for (int i = 0; i < kButtons; ++i) {
        const bool pressed = down(kInputs[i].pin);
        armed[i] = armed[i] || !pressed;
        if (const auto event = debouncers[i].update(armed[i] && pressed, now_ms)) {
            out[count++] = {kInputs[i].button, *event};
        }
    }
    return count;
}

bool ok_down() { return down(board::kBoard.pins.btn_ok); }

}  // namespace buttons
