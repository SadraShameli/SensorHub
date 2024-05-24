#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "Storage.h"
#include "Output.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Sound.h"

static const char *TAG = "Network";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    if (Storage::GetConfigMode())
    {
        WiFi::StartAP();

        for (;;)
        {
            Network::UpdateConfig();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    else
    {
        WiFi::StartStation();
        HTTP::Init();

        for (;;)
        {
            Network::Update();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    vTaskDelete(nullptr);
}

void Network::NotifyConfigSet()
{
    xTaskNotify(xHandle, DeviceConfig::Tasks::Notifications::ConfigSet, eSetBits);
}

void Network::Init()
{
    xTaskCreate(&vTask, TAG, 8192, nullptr, configMAX_PRIORITIES - 2, &xHandle);
}

void Network::Update()
{
    static uint32_t registerInterval = Storage::GetRegisterInterval() * 1000;
    vTaskDelay(pdMS_TO_TICKS(registerInterval));

    if (Backend::RegisterReadings())
    {
        Sound::ResetLevels();
        Output::Blink(Output::LedG, 1000);
    }
}

void Network::UpdateConfig()
{
    Output::Blink(Output::LedY, 1000, true);

    xTaskNotifyWait(0, DeviceConfig::Tasks::Notifications::ConfigSet, &DeviceConfig::Tasks::Notifications::Notification, portMAX_DELAY);

    if (DeviceConfig::Tasks::Notifications::Notification & DeviceConfig::Tasks::Notifications::ConfigSet)
    {
        HTTP::StopServer();
        WiFi::StartStation();
        HTTP::Init();

        WiFi::WaitForConnection();
        Backend::GetConfiguration();
        WiFi::StartAP();
    }
}
