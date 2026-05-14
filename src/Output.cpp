#include "Output.h"

#include <climits>
#include <cstdint>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Output {

struct OutputPin {
    gpio_num_t PinNum = GPIO_NUM_NC;
    int64_t UpdateTimeUs = 0;
    int64_t IntervalUs = 0;
    bool ContinuousMode = false;
    bool PinState = false;

    OutputPin(Outputs pin) : PinNum((gpio_num_t)pin) {}
};

static OutputPin outputPins[] = {LedR, LedY, LedG};

void Init() {
    for (const auto& pin : outputPins) {
        ESP_ERROR_CHECK(gpio_set_direction(pin.PinNum, GPIO_MODE_OUTPUT));
        gpio_set_level(pin.PinNum, 1);

        vTaskDelay(pdMS_TO_TICKS(250));
    }

    vTaskDelay(pdMS_TO_TICKS(250));

    for (const auto& pin : outputPins) {
        gpio_set_level(pin.PinNum, 0);
    }
}

void Update() {
    const int64_t nowUs = esp_timer_get_time();

    for (auto& pin : outputPins) {
        if (pin.IntervalUs == INT64_MAX) {
            continue;
        }
        if ((nowUs - pin.UpdateTimeUs) > pin.IntervalUs) {
            pin.UpdateTimeUs = nowUs;

            if (pin.ContinuousMode) {
                pin.PinState = !pin.PinState;
                gpio_set_level(pin.PinNum, pin.PinState);
            } else {
                if (pin.PinState) {
                    gpio_set_level(pin.PinNum, 1);
                    pin.PinState = false;
                } else {
                    gpio_set_level(pin.PinNum, 0);
                }
            }
        }
    }
}

void Toggle(Outputs pinNumber, bool targetPinState) {
    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            pin.IntervalUs = INT64_MAX;
            gpio_set_level(pin.PinNum, targetPinState);
            return;
        }
    }
}

void Blink(Outputs pinNumber, int64_t blinkTimeMs, bool continuous) {
    const int64_t intervalUs = blinkTimeMs * 1000LL;

    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            if (pin.IntervalUs != intervalUs) {
                pin.UpdateTimeUs = 0;
                pin.IntervalUs = intervalUs;
            }

            pin.ContinuousMode = continuous;

            if (!continuous) {
                pin.PinState = true;
            }

            return;
        }
    }
}

void SetContinuity(Outputs pinNumber, bool continuous) {
    for (auto& pin : outputPins) {
        if ((Outputs)pin.PinNum == pinNumber) {
            pin.UpdateTimeUs = 0;
            pin.ContinuousMode = continuous;
            return;
        }
    }
}

}
