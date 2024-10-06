#include <ctime>
#include <vector>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Climate.h"
#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Mic.h"
#include "Output.h"
#include "Pin.h"
#include "Storage.h"

namespace Pin
{
    static const char *TAG = "Pin";
    static TaskHandle_t xHandle = nullptr;

    static bool resetCanceled = false;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

        Input::Init();
        Output::Init();

        for (;;)
        {
            Input::Update();
            Output::Update();
            Update();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelete(nullptr);
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
    }

    void Update()
    {
        using Menus = Configuration::Menu::Menus;
        using Sensors = Configuration::Sensor::Sensors;

        if (Input::GetPinState(Input::Inputs::Up))
        {
            Output::Blink(Output::LedY);
            Display::ResetScreenSaver();

            if (!Storage::GetConfigMode())
            {
                while (!resetCanceled && clock() < 10000)
                {
                    Display::SetMenu(Menus::Reset);
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
                    Display::SetMenu(Menus::Main);
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
                if (Display::GetMenu() != Menus::Failsafe)
                    esp_restart();

                Failsafe::PopFailure();
            }

            switch (Display::GetMenu())
            {
            case Menus::Temperature:
                Climate::ResetValues(Sensors::Temperature);
                break;

            case Menus::Humidity:
                Climate::ResetValues(Sensors::Humidity);
                break;

            case Menus::AirPressure:
                Climate::ResetValues(Sensors::AirPressure);
                break;

            case Menus::GasResistance:
                Climate::ResetValues(Sensors::GasResistance);
                break;

            case Menus::Altitude:
                Climate::ResetValues(Sensors::Altitude);
                break;

            case Menus::Loudness:
            case Menus::Recording:
                Mic::ResetValues();
                break;

                // case Menus::RPM:
                //     RPM::ResetValues();
                //     break;

            case Menus::Failsafe:
                Failsafe::PopFailure();

            default:
                break;
            }
        }
    }
}