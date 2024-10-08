#include <ctime>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Backend.h"
#include "Configuration.h"
#include "Display.h"
#include "HTTP.h"
#include "Mic.h"
#include "Network.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"

namespace Network
{
    static const char *TAG = "Network";
    static TaskHandle_t xHandle = nullptr;

    static uint32_t registerInterval = 0;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

        if (Storage::GetConfigMode())
        {
            WiFi::StartAP();

            for (;;)
            {
                UpdateConfig();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        else
        {
            WiFi::StartStation();

            if (Storage::GetSensorState(Configuration::Sensor::Recording))
                vTaskDelete(nullptr);

            HTTP::Init();
            registerInterval = Storage::GetRegisterInterval() * 1000;

            for (;;)
            {
                Update();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        vTaskDelete(nullptr);
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 8192, nullptr, configMAX_PRIORITIES - 1, &xHandle);
    }

    void Update()
    {
        vTaskDelay(pdMS_TO_TICKS(registerInterval));

        if (Backend::RegisterReadings())
        {
            Mic::ResetValues();
            Output::Blink(Output::LedG, 1000);
        }
    }

    void UpdateConfig()
    {
        Output::Blink(Output::LedY, 1000, true);

        xTaskNotifyWait(0, Configuration::Notification::ConfigSet, &Configuration::Notification::Values, portMAX_DELAY);

        if (Configuration::Notification::Get(Configuration::Notification::ConfigSet))
        {
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

    void Reset()
    {
        vTaskDelete(xHandle);
        Init();
    }

    void NotifyConfigSet()
    {
        xTaskNotify(xHandle, Configuration::Notification::ConfigSet, eSetBits);
    }

}