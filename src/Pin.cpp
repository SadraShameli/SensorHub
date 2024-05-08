#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Input.h"
#include "Output.h"
#include "Storage.h"
#include "Pin.h"

static const char *TAG = "Pin";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing %s task", TAG);

    Input::Init();
    Output::Init();

    for (;;)
    {
        Input::Update();
        Output::Update();
        Pin::Update();
    }

    vTaskDelete(nullptr);
}

void Pin::Init()
{
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
}

void Pin::Update()
{
    if (Input::GetPinState(DeviceConfig::Inputs::Up))
    {
        Output::Blink(DeviceConfig::Outputs::LedY, 50);

        if (Storage::GetConfigMode())
        {
            esp_restart();
        }

        else
        {
            while (clock() < 10000)
            {
                Input::Update();
                Output::Update();

                if (Input::GetPinState(DeviceConfig::Inputs::Up))
                {
                    break;
                }

                if (Input::GetPinState(DeviceConfig::Inputs::Down))
                {
                    Storage::Reset();
                    esp_restart();
                }
            }
        }
    }

    else if (Input::GetPinState(DeviceConfig::Inputs::Down))
    {
        Output::Blink(DeviceConfig::Outputs::LedY, 50);
    }
}