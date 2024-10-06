#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Output.h"

namespace Failsafe
{
    static const char *TAG = "Failsafe";
    static TaskHandle_t xHandle = nullptr;

    static std::stack<Failure> failures;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

        for (;;)
            Update();

        vTaskDelete(nullptr);
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 1, &xHandle);
    }

    void Update()
    {
        ESP_LOGI(TAG, "Waiting for failure");

        xTaskNotifyWait(0, Configuration::Notification::NewFailsafe, &Configuration::Notification::Values, portMAX_DELAY);

        const Failure &topFailure = failures.top();
        ESP_LOGE(TAG, "%s - %s", topFailure.Caller, topFailure.Message.c_str());

        Output::Blink(Output::LedR, 5000);
        Display::SetMenu(Configuration::Menu::Failsafe);
    }

    void AddFailure(const char *caller, std::string &&message)
    {
        if (failures.size() > 24)
        {
            failures.pop();
            ESP_LOGI(TAG, "Popped failure");
        }

        failures.emplace(caller, std::move(message));
        ESP_LOGI(TAG, "Pushed failure - current size: %d", failures.size());

        xTaskNotify(xHandle, Configuration::Notification::NewFailsafe, eSetBits);
    }

    void AddFailureDelayed(const char *caller, std::string &&message)
    {
        AddFailure(caller, std::move(message));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    void PopFailure()
    {
        if (!failures.empty())
        {
            failures.pop();
            ESP_LOGI(TAG, "Popped failure");
        }
    }

    const std::stack<Failure> &GetFailures() { return failures; }
}
