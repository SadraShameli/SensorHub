#include <stack>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Failsafe.h"
#include "Output.h"

static const char *TAG = "Failsafe";
static TaskHandle_t xHandle = nullptr;
static std::stack<Failsafe::Failure> failures;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    for (;;)
    {
        Failsafe::Update();
        taskYIELD();
    }

    vTaskDelete(nullptr);
}

void Failsafe::AddFailure(const char *caller, const std::string &failure)
{
    if (failures.size() > 9)
    {
        failures.pop();
        ESP_LOGI(TAG, "Popped failure");
    }

    failures.push(Failsafe::Failure(caller, failure));
    ESP_LOGI(TAG, "Pushed failure - current size: %d", failures.size());

    xTaskNotify(xHandle, DeviceConfig::Tasks::Notifications::NewFailsafe, eSetBits);
}

void Failsafe::Init()
{
    xTaskCreate(&vTask, TAG, 4096, nullptr, configMAX_PRIORITIES - 1, &xHandle);
}

void Failsafe::Update()
{
    ESP_LOGI(TAG, "Waiting for failure");

    xTaskNotifyWait(0, DeviceConfig::Tasks::Notifications::NewFailsafe, &DeviceConfig::Tasks::Notifications::Notification, portMAX_DELAY);

    const Failsafe::Failure &topFailure = failures.top();
    ESP_LOGE(TAG, "Failure received from %s: %s", topFailure.Caller, topFailure.Message.c_str());

    Output::Blink(Output::LedR, 5000);
}
