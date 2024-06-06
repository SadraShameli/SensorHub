#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "Configuration.h"
#include "Storage.h"
#include "Output.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Display.h"
#include "Sound.h"

static const char *TAG = "Network";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *arg)
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

void Network::Init()
{
    xTaskCreate(&vTask, TAG, 8192, nullptr, configMAX_PRIORITIES - 1, &xHandle);
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

    xTaskNotifyWait(0, Configuration::Notifications::ConfigSet, &Configuration::Notifications::Notification, portMAX_DELAY);

    if (Configuration::Notifications::Get(Configuration::Notifications::ConfigSet))
    {
        Display::SetMenu(Configuration::Menus::ConfigConnecting);
        HTTP::StopServer();
        WiFi::StartStation();
        HTTP::Init();

        WiFi::WaitForConnection();
        Display::SetMenu(Configuration::Menus::ConfigConnected);

        Backend::GetConfiguration();
        WiFi::StartAP();
    }
}

void Network::NotifyConfigSet()
{
    xTaskNotify(xHandle, Configuration::Notifications::ConfigSet, eSetBits);
}
