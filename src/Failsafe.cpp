#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "Output.h"
#include "Display.h"

static const char *TAG = "Failsafe";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    for (;;)
    {
        Failsafe::Update();
    }

    vTaskDelete(nullptr);
}

void Failsafe::Init()
{
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 1, &xHandle);
}

void Failsafe::Update()
{
    ESP_LOGI(TAG, "Waiting for failure");

    xTaskNotifyWait(0, Configuration::Notifications::NewFailsafe, &Configuration::Notifications::Notification, portMAX_DELAY);

    const Failsafe::Failure &topFailure = m_Failures.top();
    ESP_LOGE(TAG, "Failure received from %s: %s", topFailure.Caller, topFailure.Message.c_str());

    Output::Blink(Output::LedR, 5000);
    Display::SetMenu(Configuration::Menus::Failsafe);
}

void Failsafe::AddFailure(const char *caller, const std::string &failure)
{
    if (m_Failures.size() > 24)
    {
        m_Failures.pop();
        ESP_LOGI(TAG, "Popped failure");
    }

    m_Failures.push(Failsafe::Failure(caller, failure));
    ESP_LOGI(TAG, "Pushed failure - current size: %d", m_Failures.size());

    xTaskNotify(xHandle, Configuration::Notifications::NewFailsafe, eSetBits);
}

void Failsafe::PopFailure()
{
    if (!m_Failures.empty())
    {
        ESP_LOGI(TAG, "Popped failure");
        m_Failures.pop();
    }
}