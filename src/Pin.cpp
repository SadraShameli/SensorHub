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

namespace Pin
{
    static const char *TAG = "Pin";
    static TaskHandle_t xHandle = nullptr;

    static bool resetCanceled = false;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing task");

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
        if (Input::GetPinState(Input::Inputs::Up))
        {
            Output::Blink(Output::LedY);
            Display::ResetScreenSaver();

            if (!Storage::GetConfigMode())
            {
                while (!resetCanceled && clock() < 10000)
                {
                    Display::SetMenu(Configuration::Menu::Reset);
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
                    Display::SetMenu(Configuration::Menu::Main);
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
                if (Display::GetMenu() != Configuration::Menu::Failsafe)
                    esp_restart();

                Failsafe::PopFailure();
            }

            switch (Display::GetMenu())
            {
            case Configuration::Menu::Temperature:
                Climate::ResetValues(Configuration::Sensor::Temperature);
                break;

            case Configuration::Menu::Humidity:
                Climate::ResetValues(Configuration::Sensor::Humidity);
                break;

            case Configuration::Menu::AirPressure:
                Climate::ResetValues(Configuration::Sensor::AirPressure);
                break;

            case Configuration::Menu::GasResistance:
                Climate::ResetValues(Configuration::Sensor::GasResistance);
                break;

            case Configuration::Menu::Altitude:
                Climate::ResetValues(Configuration::Sensor::Altitude);
                break;

            case Configuration::Menu::Loudness:
                Sound::ResetLevels();
                break;

                // case Menus::RPM:
                //     RPM::ResetValues();
                //     break;
            case Configuration::Menu::Failsafe:
                Failsafe::PopFailure();

            default:
                break;
            }
        }
    }
}