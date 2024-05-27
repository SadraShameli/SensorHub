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

static const char *TAG = "Gui";
static TaskHandle_t xHandle = nullptr;

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    Display::Init();

    for (;;)
    {
        Display::Update();
        Gui::Update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(nullptr);
}

void Gui::Init()
{
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 3, &xHandle);
}

void Gui::Update()
{
    switch (Display::GetMenu())
    {
    case Configuration::Menus::Main:
    {
        Display::PrintMain();
        return;
    }

    case Configuration::Menus::Failsafe:
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

    case Configuration::Menus::Config:
    {
        if (!Configuration::Notifications::Get(Configuration::Notifications::ConfigSet))
            Display::PrintLines("Configuration", ("SSID: " + std::string(Configuration::WiFi::SSID)).c_str(), "Server IP:", WiFi::GetIPAP().c_str());
        return;
    }

    case Configuration::Menus::ConfigConnecting:
    {
        Display::PrintLines("Configuration", ("Connecting to " + Storage::GetSSID()).c_str(), "", "");
        return;
    }

    case Configuration::Menus::ConfigConnected:
    {
        Display::PrintLines("Configuration", ("Connected to " + Storage::GetSSID()).c_str(), "", "Retrieving data");
        return;
    }

    case Configuration::Menus::ConfigClients:
    {
        Display::PrintWiFiClients();
        return;
    }

    case Configuration::Menus::Reset:
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
            case Configuration::Menus::Temperature:
            {
                Display::PrintTemperature();
                return;
            }

            case Configuration::Menus::Humidity:
            {
                Display::PrintHumidity();
                return;
            }

            case Configuration::Menus::AirPressure:
            {
                Display::PrintAirPressure();
                return;
            }

            case Configuration::Menus::GasResistance:
            {
                Display::PrintGasResistance();
                return;
            }

            case Configuration::Menus::Altitude:
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
            if (Display::GetMenu() == Configuration::Menus::Loudness)
                Display::PrintLoudness();
        }

        // if (Display::GetMenu() == Configuration::Menus::RPM)
        //     Display::PrintRPM();

        return;
    }
    }
}

void Gui::Pause()
{
    vTaskSuspend(xHandle);
}

void Gui::Resume()
{
    vTaskResume(xHandle);
}