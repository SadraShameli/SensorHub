#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Output.h"

struct OutputPin
{
    gpio_num_t PinNum = GPIO_NUM_NC;
    clock_t UpdateTime = 0, Interval = 0;
    bool ContinuousMode = false, PinState = false;

    OutputPin(Output::Outputs pin) : PinNum((gpio_num_t)pin) {}
};
static OutputPin outputPins[] = {Output::LedR, Output::LedY, Output::LedG};

void Output::Init()
{
    for (auto &pin : outputPins)
    {
        gpio_set_direction(pin.PinNum, GPIO_MODE_OUTPUT);
        gpio_set_level(pin.PinNum, 1);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    for (auto &pin : outputPins)
    {
        gpio_set_level(pin.PinNum, 0);
    }
}

void Output::Update()
{
    clock_t currentTime = clock();

    for (auto &pin : outputPins)
    {
        if ((currentTime - pin.UpdateTime) > pin.Interval)
        {
            pin.UpdateTime = currentTime;

            if (pin.ContinuousMode)
            {
                pin.PinState = !pin.PinState;
                gpio_set_level(pin.PinNum, pin.PinState);
            }

            else
            {
                if (pin.PinState)
                {
                    gpio_set_level(pin.PinNum, 1);
                    pin.PinState = false;
                }

                else
                {
                    gpio_set_level(pin.PinNum, 0);
                }
            }
        }
    }
}

void Output::Toggle(Outputs pinNumber, bool targetPinState)
{
    for (auto &pin : outputPins)
    {
        if ((Outputs)pin.PinNum == pinNumber)
        {
            pin.Interval = ULONG_MAX;
            gpio_set_level(pin.PinNum, targetPinState);
            return;
        }
    }
}

void Output::Blink(Outputs pinNumber, clock_t blinkTime, bool continuousModeBlinking)
{
    for (auto &pin : outputPins)
    {
        if ((Outputs)pin.PinNum == pinNumber)
        {
            if (pin.Interval != blinkTime)
            {
                pin.UpdateTime = 0;
                pin.Interval = blinkTime;
            }

            pin.ContinuousMode = continuousModeBlinking;

            if (!continuousModeBlinking)
            {
                pin.PinState = true;
            }

            return;
        }
    }
}

void Output::SetContinuity(Outputs pinNumber, bool continuousModeBlinking)
{
    for (auto &pin : outputPins)
    {
        if ((Outputs)pin.PinNum == pinNumber)
        {
            pin.UpdateTime = 0;
            pin.ContinuousMode = continuousModeBlinking;
            return;
        }
    }
}
