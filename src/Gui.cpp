#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Storage.h"
#include "WiFi.h"
#include "Climate.h"
#include "Mic.h"
#include "Gui.h"

namespace Gui
{
    static const char *TAG = "Gui";
    static TaskHandle_t xHandle = nullptr;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

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
        using Menus = Configuration::Menu::Menus;

        switch (Display::GetMenu())
        {
        case Menus::Main:
            Display::PrintMain();
            break;

        case Menus::Failsafe:
        {
            const auto &failures = Failsafe::GetFailures();
            if (failures.empty())
                Display::PrintText("Failsafe", "There are currently  no failures.");
            else
            {
                const auto &failure = failures.top();
                Display::PrintText(("Failsafe: " + std::string(failure.Caller)).c_str(), failure.Message.c_str());
            }
            break;
        }

        case Menus::Config:
            if (!Configuration::Notification::Get(Configuration::Notification::ConfigSet))
                Display::PrintLines("Configuration", ("SSID: " + std::string(Configuration::WiFi::SSID)).c_str(), "Server IP: ", WiFi::GetIPAP().c_str());
            break;

        case Menus::ConfigConnecting:
            Display::PrintLines("Configuration", ("Connecting to " + Storage::GetSSID()).c_str(), "", "");
            break;

        case Menus::ConfigConnected:
            Display::PrintLines("Configuration", ("Connected to " + Storage::GetSSID()).c_str(), "", "Retrieving data");
            break;

        case Menus::ConfigClients:
            Display::PrintWiFiClients();
            break;

        case Menus::Reset:
            Display::PrintText("Configuration", "Press bottom button  to reset device");
            break;

        default:
            if (Climate::IsOK())
            {
                switch (Display::GetMenu())
                {
                case Menus::Temperature:
                    Display::PrintTemperature();
                    break;

                case Menus::Humidity:
                    Display::PrintHumidity();
                    break;

                case Menus::AirPressure:
                    Display::PrintAirPressure();
                    break;

                case Menus::GasResistance:
                    Display::PrintGasResistance();
                    break;

                case Menus::Altitude:
                    Display::PrintAltitude();
                    break;

                default:
                    break;
                }
            }

            if (Mic::IsOK())
            {
                switch (Display::GetMenu())
                {
                case Menus::Loudness:
                case Menus::Recording:
                    Display::PrintLoudness();

                default:
                    break;
                }
            }

            // if (Display::GetMenu() == Menus::RPM)
            //     Display::PrintRPM();
        }
    }

    void Pause() { vTaskSuspend(xHandle); }
    void Resume() { vTaskResume(xHandle); }
}
