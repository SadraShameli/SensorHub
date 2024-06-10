#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Configuration.h"
#include "Display.h"
#include "Gui.h"
#include "Failsafe.h"
#include "Input.h"
#include "Storage.h"
#include "WiFi.h"
#include "Climate.h"
#include "Sound.h"

namespace Gui
{
    static const char *TAG = "Gui";
    static TaskHandle_t xHandle = nullptr;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing task");

        Display::Init();

        for (;;)
        {
            Display::Update();
            Update();

            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelete(nullptr);
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 3, &xHandle);
    }

    void Update()
    {
        switch (Display::GetMenu())
        {
        case Configuration::Menu::Main:
        {
            Display::PrintMain();
            return;
        }

        case Configuration::Menu::Failsafe:
        {
            const auto &failures = Failsafe::GetFailures();
            if (failures.empty())
                Display::PrintText("Failsafe", "There are currently  no failures.");
            else
            {
                const auto &failure = failures.top();
                Display::PrintText(("Failsafe: " + std::string(failure.Caller)).c_str(), failure.Message.c_str());
            }
            return;
        }

        case Configuration::Menu::Config:
        {
            if (!Configuration::Notification::Get(Configuration::Notification::ConfigSet))
                Display::PrintLines("Configuration", ("SSID: " + std::string(Configuration::WiFi::SSID)).c_str(), "Server IP:", WiFi::GetIPAP().c_str());
            return;
        }

        case Configuration::Menu::ConfigConnecting:
        {
            Display::PrintLines("Configuration", ("Connecting to " + Storage::GetSSID()).c_str(), "", "");
            return;
        }

        case Configuration::Menu::ConfigConnected:
        {
            Display::PrintLines("Configuration", ("Connected to " + Storage::GetSSID()).c_str(), "", "Retrieving data");
            return;
        }

        case Configuration::Menu::ConfigClients:
        {
            Display::PrintWiFiClients();
            return;
        }

        case Configuration::Menu::Reset:
        {
            Display::PrintText("Configuration", "Press bottom button  to reset device");
            return;
        }

        default:
        {
            if (Climate::IsOK())
            {
                switch (Display::GetMenu())
                {
                case Configuration::Menu::Temperature:
                {
                    Display::PrintTemperature();
                    return;
                }

                case Configuration::Menu::Humidity:
                {
                    Display::PrintHumidity();
                    return;
                }

                case Configuration::Menu::AirPressure:
                {
                    Display::PrintAirPressure();
                    return;
                }

                case Configuration::Menu::GasResistance:
                {
                    Display::PrintGasResistance();
                    return;
                }

                case Configuration::Menu::Altitude:
                {
                    Display::PrintAltitude();
                    return;
                }

                default:
                    break;
                }
            }

            if (Sound::IsOK())
            {
                if (Display::GetMenu() == Configuration::Menu::Loudness)
                    Display::PrintLoudness();
            }

            // if (Display::GetMenu() == Configuration::Menu::RPM)
            //     Display::PrintRPM();

            return;
        }
        }
    }

    void Pause() { vTaskSuspend(xHandle); }
    void Resume() { vTaskResume(xHandle); }
}
