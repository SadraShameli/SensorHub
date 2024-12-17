#include "Pin.h"

#include <ctime>
#include <vector>

#include "Climate.h"
#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Mic.h"
#include "Output.h"
#include "Storage.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Pin {

static const char* TAG = "Pin";
static TaskHandle_t xHandle = nullptr;

static bool resetCanceled = false;

/**
 * @brief Task function that initializes input and output modules and
 * continuously updates them.
 *
 * This function is intended to be run as a FreeRTOS task. It performs the
 * following steps:
 *
 * 1. Logs an initialization message.
 *
 * 2. Initializes the Input and Output modules.
 *
 * 3. Enters an infinite loop where it:
 *    - Updates the Input module.
 *
 *    - Updates the Output module.
 *
 *    - Calls the Update function.
 *
 *    - Delays for 10 milliseconds.
 *
 * @param arg Pointer to the task's argument (unused).
 */
static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    Input::Init();
    Output::Init();

    for (;;) {
        Input::Update();
        Output::Update();
        Update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Initializes the Pin module.
 *
 * This function creates a FreeRTOS task to run the `vTask` function.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
}

/**
 * @brief Updates the Pin module.
 *
 * This function checks if the `Up` or `Down` input pins are pressed and
 * performs the following actions:
 *
 * - If the `Up` pin is pressed, it advances to the next menu.
 *
 * - If the `Down` pin is pressed, it resets the values of the current menu.
 */
void Update() {
    using Menus = Configuration::Menu::Menus;
    using Sensors = Configuration::Sensor::Sensors;

    if (Input::GetPinState(Input::Inputs::Up)) {
        Output::Blink(Output::LedY);
        Display::ResetScreenSaver();

        if (!Storage::GetConfigMode()) {
            while (!resetCanceled && clock() < 10000) {
                Display::SetMenu(Menus::Reset);
                Input::Update();
                Output::Update();

                if (Input::GetPinState(Input::Up)) {
                    resetCanceled = true;
                    return;
                }

                if (Input::GetPinState(Input::Inputs::Down)) {
                    Storage::Reset();
                    esp_restart();
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (!resetCanceled) {
                Display::SetMenu(Menus::Main);
                resetCanceled = true;
            }
        }

        Display::NextMenu();
    }

    else if (Input::GetPinState(Input::Inputs::Down)) {
        Output::Blink(Output::LedY);
        Display::ResetScreenSaver();

        if (Storage::GetConfigMode()) {
            if (Display::GetMenu() != Menus::Failsafe) {
                esp_restart();
            }

            Failsafe::PopFailure();
        }

        switch (Display::GetMenu()) {
            case Menus::Temperature:
                Climate::ResetValues(Sensors::Temperature);
                break;

            case Menus::Humidity:
                Climate::ResetValues(Sensors::Humidity);
                break;

            case Menus::AirPressure:
                Climate::ResetValues(Sensors::AirPressure);
                break;

            case Menus::GasResistance:
                Climate::ResetValues(Sensors::GasResistance);
                break;

            case Menus::Altitude:
                Climate::ResetValues(Sensors::Altitude);
                break;

            case Menus::Loudness:
            case Menus::Recording:
                Mic::ResetValues();
                break;

            case Menus::Failsafe:
                Failsafe::PopFailure();

            default:
                break;
        }
    }
}

}  // namespace Pin