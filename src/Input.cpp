#include "driver/gpio.h"

#include "Input.h"

namespace Input
{
    struct InputPin
    {
        gpio_num_t PinNumber = GPIO_NUM_NC;
        bool PinState = false, IsLocked = false;
        InputPin(Inputs _PinNumber) : PinNumber((gpio_num_t)_PinNumber) {}
    };

    static InputPin inputPins[] = {Up, Down};

    void Init()
    {
        for (const auto &pin : inputPins)
        {
            ESP_ERROR_CHECK(gpio_set_direction(pin.PinNumber, GPIO_MODE_INPUT));
            ESP_ERROR_CHECK(gpio_set_pull_mode(pin.PinNumber, GPIO_PULLUP_ONLY));
        }
    }

    void Update()
    {
        for (auto &pin : inputPins)
        {
            bool pinPinState = gpio_get_level(pin.PinNumber);

            if (pinPinState && pin.IsLocked)
            {
                pin.PinState = false;
                pin.IsLocked = false;
            }

            else if (!pinPinState && !pin.IsLocked)
            {
                pin.IsLocked = true;
                pin.PinState = true;
            }
        }
    }

    bool GetPinState(Inputs pinNumber)
    {
        for (auto &pin : inputPins)
        {
            if ((Inputs)pin.PinNumber == pinNumber && pin.PinState)
            {
                pin.PinState = false;
                return true;
            }
        }

        return false;
    }
}
