#include "Output.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Output {

/**
 * @struct
 * @brief Structure to hold the state of an input pin.
 */
struct OutputPin {
    gpio_num_t PinNum = GPIO_NUM_NC;
    clock_t UpdateTime = 0, Interval = 0;
    bool ContinuousMode = false, PinState = false;

    OutputPin(Outputs pin) : PinNum((gpio_num_t)pin) {}
};

static OutputPin outputPins[] = {LedR, LedY, LedG};

/**
 * @brief Initializes the output pins.
 *
 * This function sets the direction of each pin in the outputPins array to
 * output mode, sets the initial level of each pin to high (1), and then delays
 * for 250 milliseconds. After the delay, it sets the level of each pin to low
 * (0).
 */
void Init() {
    for (const auto& pin : outputPins) {
        ESP_ERROR_CHECK(gpio_set_direction(pin.PinNum, GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, 1));

        vTaskDelay(pdMS_TO_TICKS(250));
    }

    vTaskDelay(pdMS_TO_TICKS(250));

    for (const auto& pin : outputPins) {
        ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, 0));
    }
}

/**
 * @brief Updates the state of the output pins.
 *
 * This function checks if the interval has elapsed for each pin in the
 * outputPins array. If the interval has elapsed, it toggles the state of the
 * pin. If the pin is in continuous mode, it toggles the state of the pin every
 * interval.
 */
void Update() {
    clock_t currentTime = clock();

    for (auto& pin : outputPins) {
        if ((currentTime - pin.UpdateTime) > pin.Interval) {
            pin.UpdateTime = currentTime;

            if (pin.ContinuousMode) {
                pin.PinState = !pin.PinState;
                ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, pin.PinState));
            }

            else {
                if (pin.PinState) {
                    ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, 1));
                    pin.PinState = false;
                }

                else {
                    ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, 0));
                }
            }
        }
    }
}

/**
 * @brief Toggles the state of the specified output pin.
 *
 * This function sets the interval of the specified pin to the maximum value
 * and sets the level of the pin to the target state.
 *
 * @param pinNumber The output pin to toggle.
 * @param targetPinState The target state of the pin.
 */
void Toggle(Outputs pinNumber, bool targetPinState) {
    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            pin.Interval = ULONG_MAX;
            ESP_ERROR_CHECK(gpio_set_level(pin.PinNum, targetPinState));
            return;
        }
    }
}

/**
 * @brief Blinks the specified output pin.
 *
 * This function sets the interval of the specified pin to the specified blink
 * time and sets the pin to continuous mode if specified.
 *
 * @param pinNumber The output pin to blink.
 * @param blinkTime The time to blink the pin.
 * @param continuousModeBlinking Whether to blink the pin continuously.
 */
void Blink(Outputs pinNumber, clock_t blinkTime, bool continuousModeBlinking) {
    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            if (pin.Interval != blinkTime) {
                pin.UpdateTime = 0;
                pin.Interval = blinkTime;
            }

            pin.ContinuousMode = continuousModeBlinking;

            if (!continuousModeBlinking) {
                pin.PinState = true;
            }

            return;
        }
    }
}

/**
 * @brief Sets the continuity of the specified output pin.
 *
 * This function sets the update time of the specified pin to 0 and sets the
 * continuous mode of the pin to the specified value.
 *
 * @param pinNumber The output pin to set the continuity.
 * @param continuousModeBlinking Whether to blink the pin continuously.
 */
void SetContinuity(Outputs pinNumber, bool continuousModeBlinking) {
    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            pin.UpdateTime = 0;
            pin.ContinuousMode = continuousModeBlinking;
            return;
        }
    }
}

}  // namespace Output