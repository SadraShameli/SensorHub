#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Sound.h"

static const char *TAG = "Network";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing %s task", TAG);

    WiFi::Init();
    WiFi::StartAP();

    for (;;)
    {
        Network::Update();
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
    Output::Blink(DeviceConfig::Outputs::LedY, 1000, true);

    xTaskNotifyWait(0, DeviceConfig::Tasks::Notifications::ConfigSet, &DeviceConfig::Tasks::Notifications::Notification, pdMS_TO_TICKS(25));

    if (DeviceConfig::Tasks::Notifications::Notification & DeviceConfig::Tasks::Notifications::ConfigSet)
    {
        HTTP::StopServer();
        WiFi::StartStation();
        HTTP::Init();

        for (;;)
        {
            Output::Blink(DeviceConfig::Outputs::LedY, 250, true);

            if (WiFi::IsConnected())
            {
                if (Backend::GetConfiguration())
                {
                    Storage::SetSSID(Backend::SSID);
                    Storage::SetPassword(Backend::Password);
                    Storage::SetDeviceId(Backend::DeviceId);
                    Storage::SetDeviceName(Backend::DeviceName);
                    Storage::SetAuthKey(Backend::AuthKey);
                    Storage::SetAddress(Backend::Address);
                    Storage::SetLoudnessThreshold(Backend::LoudnessThreshold);
                    Storage::SetRegisterInterval(Backend::RegisterInterval);
                    Storage::SetConfigMode(false);
                    Storage::Commit();

                    esp_restart();
                }

                WiFi::StartAP();
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }
}
