#include "driver/gpio.h"
#include "Input.h"

struct InputPin
{
    gpio_num_t PinNumber = GPIO_NUM_NC;
    bool PinState = false, IsLocked = false;

    InputPin(Input::Inputs _PinNumber) : PinNumber((gpio_num_t)_PinNumber) {}
};

static InputPin inputPins[] = {Input::Up, Input::Down};

void Input::Init()
{
    for (auto &pin : inputPins)
    {
        gpio_set_direction(pin.PinNumber, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin.PinNumber, GPIO_PULLUP_ONLY);
    }
}

void Input::Update()
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

bool Input::GetPinState(Inputs pinNumber)
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
