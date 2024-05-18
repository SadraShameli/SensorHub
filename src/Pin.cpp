#include <ctime>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Pin.h"
#include "Storage.h"
#include "Input.h"
#include "Output.h"

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
        taskYIELD();
    }

    vTaskDelete(nullptr);
}

void Pin::Init()
{
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
}

void Pin::Update()
{
    if (Input::GetPinState(Input::Inputs::Up))
    {
        Output::Blink(Output::LedY);

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

                if (Input::GetPinState(Input::Inputs::Down))
                {
                    Storage::Reset();
                    esp_restart();
                }
            }
        }
    }

    else if (Input::GetPinState(Input::Inputs::Down))
    {
        Output::Blink(Output::LedY);
    }
}