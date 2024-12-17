#include "Network.h"

#include <ctime>

#include "Backend.h"
#include "Configuration.h"
#include "Display.h"
#include "HTTP.h"
#include "Mic.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Network {

static const char* TAG = "Network";
static TaskHandle_t xHandle = nullptr;

static uint32_t registerInterval = 0;

/**
 * @brief Task function to initialize and manage network operations.
 *
 * This function initializes the network based on the configuration mode.
 * If the device is in configuration mode, it starts the WiFi Access Point (AP)
 * and continuously updates the configuration.
 * If the device is not in configuration mode, it starts the WiFi Station,
 * checks the sensor state, initializes the HTTP server, and continuously
 * updates the network operations.
 *
 * @param arg Pointer to the task argument (unused).
 */
static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    if (Storage::GetConfigMode()) {
        WiFi::StartAP();

        for (;;) {
            UpdateConfig();

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    else {
        WiFi::StartStation();

        if (Storage::GetSensorState(Configuration::Sensor::Recording)) {
            vTaskDelete(nullptr);
        }

        HTTP::Init();
        registerInterval = Storage::GetRegisterInterval() * 1000;

        for (;;) {
            Update();

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Initializes the network task.
 *
 * This function creates a FreeRTOS task for handling network operations.
 * The task is created with the highest priority and a stack size of 8192 bytes.
 *
 * @note The task handle is stored in the global variable `xHandle`.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 8192, nullptr, configMAX_PRIORITIES - 1, &xHandle);
}

/**
 * @brief Updates the network operations.
 *
 * This function sends the readings to the backend server and resets the values
 * if the registration is successful.
 */
void Update() {
    vTaskDelay(pdMS_TO_TICKS(registerInterval));

    if (Backend::RegisterReadings()) {
        Mic::ResetValues();

        Output::Blink(Output::LedG, 1000);
    }
}

/**
 * @brief Updates the configuration.
 *
 * This function waits for a notification to update the configuration.
 * It sets the display menu to `ConfigConnecting`, stops the HTTP server,
 * starts the WiFi Station, initializes the HTTP server, waits for the WiFi
 * connection, sets the display menu to `ConfigConnected`, gets the
 * configuration from the backend, and starts the WiFi Access Point (AP).
 */
void UpdateConfig() {
    Output::Blink(Output::LedY, 1000, true);

    xTaskNotifyWait(0, Configuration::Notification::ConfigSet,
                    &Configuration::Notification::Values, portMAX_DELAY);

    if (Configuration::Notification::Get(
            Configuration::Notification::ConfigSet)) {
        Display::SetMenu(Configuration::Menu::ConfigConnecting);
        HTTP::StopServer();
        WiFi::StartStation();
        HTTP::Init();

        WiFi::WaitForConnection();
        Display::SetMenu(Configuration::Menu::ConfigConnected);

        Backend::GetConfiguration();
        WiFi::StartAP();
    }
}

/**
 * @brief Resets the network task.
 *
 * This function deletes the current task and initializes a new network task.
 */
void Reset() {
    vTaskDelete(xHandle);
    Init();
}

/**
 * @brief Notifies the network task that the configuration has been set.
 */
void NotifyConfigSet() {
    xTaskNotify(xHandle, Configuration::Notification::ConfigSet, eSetBits);
}

}  // namespace Network