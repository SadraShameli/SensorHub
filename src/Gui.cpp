#include "Gui.h"

#include "Climate.h"
#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Mic.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace Gui {

static const char* TAG = "Gui";
static TaskHandle_t xHandle = nullptr;

/**
 * @brief Task function for initializing and updating the display.
 *
 * This function initializes the display and enters an infinite loop where it
 * continuously updates the display and performs other update operations. The
 * task delays for 10 milliseconds in each iteration of the loop.
 *
 * @param arg Pointer to the arguments passed to the task (unused).
 */
static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    Display::Init();

    for (;;) {
        Display::Update();
        Update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Initializes the task for the SensorHub GUI.
 *
 * This function creates a new task for the SensorHub GUI using the FreeRTOS
 * `xTaskCreate` function. The task is created with a stack size of 8192 bytes
 * and a priority of `tskIDLE_PRIORITY + 3`.
 *
 * @note The task handle is stored in the xHandle variable.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 3, &xHandle);
}

/**
 * @brief Updates the display based on the current menu state.
 *
 * This function checks the current menu state and updates the display
 * accordingly. It handles various menus.
 */
void Update() {
    using Menus = Configuration::Menu::Menus;

    switch (Display::GetMenu()) {
        case Menus::Main:
            Display::PrintMain();
            break;

        case Menus::Failsafe: {
            const auto& failures = Failsafe::GetFailures();

            if (failures.empty()) {
                Display::PrintText("Failsafe",
                                   "There are currently  no failures.");
            }

            else {
                const auto& failure = failures.top();
                Display::PrintText(
                    ("Failsafe: " + std::string(failure.Caller)).c_str(),
                    failure.Message.c_str());
            }

            break;
        }

        case Menus::Config:
            if (!Configuration::Notification::Get(
                    Configuration::Notification::ConfigSet)) {
                Display::PrintLines(
                    "Configuration",
                    ("SSID: " + std::string(Configuration::WiFi::SSID)).c_str(),
                    "Server IP: ",
                    WiFi::GetIPAP().c_str());
            }

            break;

        case Menus::ConfigConnecting:
            Display::PrintLines("Configuration",
                                ("Connecting to " + Storage::GetSSID()).c_str(),
                                "",
                                "");
            break;

        case Menus::ConfigConnected:
            Display::PrintLines("Configuration",
                                ("Connected to " + Storage::GetSSID()).c_str(),
                                "",
                                "Retrieving data");
            break;

        case Menus::ConfigClients:
            Display::PrintWiFiClients();
            break;

        case Menus::Reset:
            Display::PrintText("Configuration",
                               "Press bottom button  to reset device");
            break;

        default:
            if (Climate::IsOK()) {
                switch (Display::GetMenu()) {
                    case Menus::Temperature:
                        Display::PrintTemperature();
                        break;

                    case Menus::Humidity:
                        Display::PrintHumidity();
                        break;

                    case Menus::AirPressure:
                        Display::PrintAirPressure();
                        break;

                    case Menus::GasResistance:
                        Display::PrintGasResistance();
                        break;

                    case Menus::Altitude:
                        Display::PrintAltitude();
                        break;

                    default:
                        break;
                }
            }

            if (Mic::IsOK()) {
                switch (Display::GetMenu()) {
                    case Menus::Loudness:
                    case Menus::Recording:
                        Display::PrintLoudness();

                    default:
                        break;
                }
            }
    }
}

/**
 * @brief Suspends the task associated with the handle xHandle.
 *
 * This function pauses the execution of the task referenced by `xHandle`
 * by calling `vTaskSuspend`. The task will remain in the suspended state
 * until it is resumed by another function.
 */
void Pause() {
    vTaskSuspend(xHandle);
}

/**
 * @brief Resumes the task associated with the handle `xHandle`.
 *
 * This function resumes the execution of a task that was previously suspended.
 * It uses the FreeRTOS API function `vTaskResume` to achieve this.
 */
void Resume() {
    vTaskResume(xHandle);
}

}  // namespace Gui
