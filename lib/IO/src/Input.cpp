#include "driver/gpio.h"
#include "Input.h"

struct InputPin
{
    gpio_num_t PinNum = GPIO_NUM_NC;
    bool PinState = false, IsLocked = false;

    InputPin(int pin) : PinNum((gpio_num_t)pin) {}
};

static InputPin inputPins[] UNIT_INPUT_PINS;

void Input::Init()
{
    for (auto &pin : inputPins)
    {
        gpio_set_direction(pin.PinNum, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin.PinNum, GPIO_PULLUP_ONLY);
    }
}

bool Input::GetPinState(DeviceConfig::Inputs pinNumber)
{
    for (auto &pin : inputPins)
    {
        if ((DeviceConfig::Inputs)pin.PinNum == pinNumber && pin.PinState)
        {
            pin.PinState = false;

            return true;
        }
    }

    return false;
}

void Input::Update()
{
    for (auto &pin : inputPins)
    {
        bool pinPinState = gpio_get_level(pin.PinNum);

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