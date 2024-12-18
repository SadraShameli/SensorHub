#include "Failsafe.h"

#include "Configuration.h"
#include "Display.h"
#include "Output.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace Failsafe {

static const char* TAG = "Failsafe";
static TaskHandle_t xHandle = nullptr;

static std::stack<Failure> failures;

/**
 * @brief Task function that runs indefinitely, calling the Update function in a
 * loop.
 *
 * This function logs an initialization message and then enters an infinite loop
 * where it continuously calls the Update function. The task is deleted when the
 * loop exits.
 *
 * @param arg Pointer to the arguments passed to the task function (unused).
 */
static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    for (;;) {
        Update();
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Initializes the failsafe mechanism by creating a task.
 *
 * This function creates a new task using the FreeRTOS `xTaskCreate` function.
 * The task is created with a stack size of 4096 bytes, a priority of
 * `tskIDLE_PRIORITY + 1`, and no parameters passed to it. The task handle
 * is stored in the `xHandle` variable.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 1, &xHandle);
}

/**
 * @brief Waits for a failure notification and handles the failure.
 *
 * This function waits indefinitely for a failure notification using
 * `xTaskNotifyWait`. Once a failure is detected, it logs the failure details,
 * blinks a red LED, and sets the display menu to the failsafe menu.
 *
 * @note This function assumes that the failures stack and the necessary
 * configurations are properly initialized and managed elsewhere in the code.
 */
void Update() {
    ESP_LOGI(TAG, "Waiting for failure");

    xTaskNotifyWait(0,
                    Configuration::Notification::NewFailsafe,
                    &Configuration::Notification::Values,
                    portMAX_DELAY);

    const Failure& topFailure = failures.top();
    ESP_LOGE(TAG, "%s - %s", topFailure.Caller, topFailure.Message.c_str());

    Output::Blink(Output::LedR, 5000);
    Display::SetMenu(Configuration::Menu::Failsafe);
}

/**
 * @brief Adds a failure message to the failures queue.
 *
 * This function adds a failure message to the failures queue. If the queue
 * size exceeds 24, the oldest failure is removed to make room for the new one.
 * It logs the addition and removal of failures and notifies a task about the
 * new failure.
 *
 * @param caller The name of the caller function or module.
 * @param message The failure message to be added.
 */
void AddFailure(const char* caller, std::string&& message) {
    if (failures.size() >= 24) {
        failures.pop();

        ESP_LOGI(TAG, "Popped failure");
    }

    failures.emplace(caller, std::move(message));

    ESP_LOGI(TAG, "Pushed failure - current size: %d", failures.size());

    xTaskNotify(xHandle, Configuration::Notification::NewFailsafe, eSetBits);
}

/**
 * @brief Adds a failure message with a delay.
 *
 * This function adds a failure message using the AddFailure function and then
 * delays the task for 10 seconds.
 *
 * @param caller The name of the caller function.
 * @param message The failure message to be added.
 */
void AddFailureDelayed(const char* caller, std::string&& message) {
    AddFailure(caller, std::move(message));

    vTaskDelay(pdMS_TO_TICKS(10000));
}

/**
 * @brief Removes the most recent failure from the failures stack.
 *
 * This function checks if the failures stack is not empty. If it contains any
 * elements, it removes the most recent failure from the stack and logs the
 * action.
 */
void PopFailure() {
    if (!failures.empty()) {
        failures.pop();

        ESP_LOGI(TAG, "Popped failure");
    }
}

/**
 * @brief Retrieves the stack of failures.
 *
 * This function returns a constant reference to the stack of Failure objects.
 * It allows read-only access to the stack, which contains the recorded
 * failures.
 *
 * @note The returned stack is not thread-safe and should be
 * accessed with caution in a multi-threaded environment.
 *
 * @return `const std::stack<Failure>&` A constant reference to the stack of
 * failures.
 */
const std::stack<Failure>& GetFailures() {
    return failures;
}

}  // namespace Failsafe
