#include <ctime>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Configuration.h"
#include "Pin.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Input.h"
#include "Output.h"
#include "Display.h"
#include "Climate.h"
#include "Sound.h"

static const char *TAG = "Pin";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    Input::Init();
    Output::Init();

    for (;;)
    {
        Input::Update();
        Output::Update();
        Pin::Update();
        vTaskDelay(pdMS_TO_TICKS(10));
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
        Display::ResetScreenSaver();

        if (!Storage::GetConfigMode())
        {
            static bool resetCanceled = false;
            while (!resetCanceled && clock() < 10000)
            {
                Display::SetMenu(Configuration::Menus::Reset);
                Input::Update();
                Output::Update();

                if (Input::GetPinState(Input::Up))
                {
                    resetCanceled = true;
                    return;
                }

                if (Input::GetPinState(Input::Inputs::Down))
                {
                    Storage::Reset();
                    esp_restart();
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (!resetCanceled)
            {
                Display::SetMenu(Configuration::Menus::Main);
                resetCanceled = true;
            }
        }

        Display::NextMenu();
    }

    else if (Input::GetPinState(Input::Inputs::Down))
    {
        Output::Blink(Output::LedY);
        Display::ResetScreenSaver();

        if (Storage::GetConfigMode())
        {
            if (Display::GetMenu() != Configuration::Menus::Failsafe)
                esp_restart();

            Failsafe::PopFailure();
        }

        switch (Display::GetMenu())
        {
        case Configuration::Menus::Temperature:
            Climate::ResetValues(Configuration::Sensors::Temperature);
            break;

        case Configuration::Menus::Humidity:
            Climate::ResetValues(Configuration::Sensors::Humidity);
            break;

        case Configuration::Menus::AirPressure:
            Climate::ResetValues(Configuration::Sensors::AirPressure);
            break;

        case Configuration::Menus::GasResistance:
            Climate::ResetValues(Configuration::Sensors::GasResistance);
            break;

        case Configuration::Menus::Altitude:
            Climate::ResetValues(Configuration::Sensors::Altitude);
            break;

        case Configuration::Menus::Loudness:
            Sound::ResetLevels();
            break;

            // case Menus::RPM:
            //     RPM::ResetValues();
            //     break;
        case Configuration::Menus::Failsafe:
            Failsafe::PopFailure();

        default:
            break;
        }
    }
}