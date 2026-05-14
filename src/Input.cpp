#include "Input.h"

#include "driver/gpio.h"
#include "esp_timer.h"

namespace Input {

static constexpr int64_t kDebounceUs = 50LL * 1000LL;

struct InputPin {
    gpio_num_t PinNumber = GPIO_NUM_NC;
    bool PinState = false, IsLocked = false;
    int64_t LastChangeUs = 0;

    InputPin(Inputs _PinNumber) : PinNumber((gpio_num_t)_PinNumber) {}
};

static InputPin inputPins[] = {Up, Down};

void Init() {
    for (const auto& pin : inputPins) {
        ESP_ERROR_CHECK(gpio_set_direction(pin.PinNumber, GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(pin.PinNumber, GPIO_PULLUP_ONLY));
    }
}

void Update() {
    const int64_t nowUs = esp_timer_get_time();

    for (auto& pin : inputPins) {
        if ((nowUs - pin.LastChangeUs) < kDebounceUs) {
            continue;
        }

        bool pinPinState = gpio_get_level(pin.PinNumber);

        if (pinPinState && pin.IsLocked) {
            pin.PinState = false;
            pin.IsLocked = false;
            pin.LastChangeUs = nowUs;
        }

        else if (!pinPinState && !pin.IsLocked) {
            pin.IsLocked = true;
            pin.PinState = true;
            pin.LastChangeUs = nowUs;
        }
    }
}

bool GetPinState(Inputs pinNumber) {
    for (auto& pin : inputPins) {
        if ((Inputs)pin.PinNumber == pinNumber && pin.PinState) {
            pin.PinState = false;

            return true;
        }
    }

    return false;
}

}
