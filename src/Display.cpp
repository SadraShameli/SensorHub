#include <ctime>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "ssd1306.h"
#include "Configuration.h"
#include "WiFi.h"
#include "Gui.h"
#include "Storage.h"
#include "Climate.h"
#include "Mic.h"
#include "Display.h"

namespace Display
{
    namespace Constants
    {
        static const uint32_t LogoDuration = 1000,
                              ScreenSaverDuration = 1 * 60 * 1000;
    };

    static const char *TAG = "Display";
    static ssd1306_handle_t dev = nullptr;

    static Configuration::Menu::Menus currentMenu = Configuration::Menu::Main;
    static clock_t currentTime = 0, prevTime = 0;
    static bool displayOff = false, isOK = false;

    void Init()
    {
        ESP_LOGI(TAG, "Initializing");

        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = GPIO_NUM_21,
            .scl_io_num = GPIO_NUM_22,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {
                .clk_speed = 1000000,
            },
        };

        ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
        ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

        dev = ssd1306_create(I2C_NUM_0, 0x3c);
        if (!dev)
        {
            ESP_LOGI(TAG, "No display detected");
            vTaskDelete(nullptr);
        }

        isOK = true;

        Clear();

        Print(35, 20, ">_sadra");
        Print(30, 40, "Sensor Hub");
        Refresh();
        vTaskDelay(pdMS_TO_TICKS(Constants::LogoDuration));

        Clear();

        if (Storage::GetConfigMode())
            currentMenu = Configuration::Menu::Config;
    }

    void Update()
    {
        currentTime = clock();
        if ((currentTime - prevTime) > Constants::ScreenSaverDuration)
        {
            prevTime = currentTime;
            displayOff = true;
            ssd1306_display_off(dev);
            Gui::Pause();
        }
    }

    void Clear() { ssd1306_clear_screen(dev, 0x00); }
    void Refresh() { ssd1306_refresh_gram(dev); }

    void ResetScreenSaver()
    {
        if (!displayOff)
            return;

        prevTime = clock();
        displayOff = false;

        ssd1306_display_on(dev);
        Gui::Resume();
    }

    void Print(uint32_t x, uint32_t y, const char *text, uint32_t size)
    {
        ssd1306_draw_string(dev, (uint8_t)x, (uint8_t)y, (const uint8_t *)text, (uint8_t)size, 1);
    }

    void PrintText(const char *header, const char *message)
    {
        Clear();

        Print(0, 0, header);
        Print(0, 16, message);

        Refresh();
    }

    void PrintLines(const char *line1, const char *line2, const char *line3, const char *line4)
    {
        Clear();

        Print(0, 0, line1);
        Print(0, 16, line2);
        Print(0, 32, line3);
        Print(0, 48, line4);

        Refresh();
    }

    void PrintMain()
    {
        const std::string &deviceName = Storage::GetDeviceName(), &ip = WiFi::GetIPStation();
        const Reading &temperature = Climate::GetTemperature(), &humidity = Climate::GetHumidity(), &loudness = Mic::GetLoudness();

        char buff[32] = {0};
        Clear();

        Print(0, 0, deviceName.c_str());

        if (WiFi::IsConnected())
            Print(0, 13, ("IP: " + ip).c_str());
        else
            Print(0, 13, "Connecting to WiFi");

        if (Climate::IsOK())
        {
            if (Storage::GetSensorState(Configuration::Sensor::Temperature))
            {
                sprintf(buff, "Temperature: %dc", (int)temperature.Current());
                Print(0, 26, buff);
            }

            if (Storage::GetSensorState(Configuration::Sensor::Humidity))
            {
                sprintf(buff, "Humidity: %d%%", (int)humidity.Current());
                Print(0, 39, buff);
            }
        }

        if (Mic::IsOK())
        {
            if (Storage::GetSensorState(Configuration::Sensor::Recording) || Storage::GetSensorState(Configuration::Sensor::Loudness))
            {
                sprintf(buff, "Loudness: %ddB", (int)loudness.Current());
                Print(0, 52, buff);
            }
        }

        Refresh();
    }

    void PrintTemperature()
    {
        const Reading &reading = Climate::GetTemperature();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Temperature");

        sprintf(buff, "%dc", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %dc", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %dc", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    void PrintHumidity()
    {
        const Reading &reading = Climate::GetHumidity();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Humidity");

        sprintf(buff, "%d%%", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %d%%", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %d%%", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    void PrintAirPressure()
    {
        const Reading &reading = Climate::GetAirPressure();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Air Pressure");

        sprintf(buff, "%d hPa", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %d hPa", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %d hPa", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    void PrintGasResistance()
    {
        const Reading &reading = Climate::GetGasResistance();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Gas Resistance");

        sprintf(buff, "%d Ohms", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %d Ohms", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %d Ohms", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    void PrintAltitude()
    {
        const Reading &reading = Climate::GetAltitude();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Altitude");

        sprintf(buff, "%dm", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %dm", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %dm", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    void PrintLoudness()
    {
        const Reading &reading = Mic::GetLoudness();

        char buff[32] = {0};
        Clear();

        Print(0, 0, "Loudness");

        sprintf(buff, "%ddB", (int)reading.Current());
        Print(0, 16, buff);

        sprintf(buff, "Max: %ddB", (int)reading.Max());
        Print(0, 32, buff);

        sprintf(buff, "Min: %ddB", (int)reading.Min());
        Print(0, 48, buff);

        Refresh();
    }

    // void PrintRPM()
    // {
    //     const Reading &reading = RPM::GetRPM();

    //     char buff[32] = {0};
    //     Clear();

    //     Print(0, 0, "RPM");

    //     sprintf(buff, "%d", (int)reading.Current());
    //     Print(0, 16, buff);

    //     sprintf(buff, "Max: %d", (int)reading.Max());
    //     Print(0, 32, buff);

    //     sprintf(buff, "Min: %d", (int)reading.Min());
    //     Print(0, 48, buff);

    //     Refresh();
    // }

    void PrintWiFiClients()
    {
        const std::vector<WiFi::ClientDetails> &clients = WiFi::GetClientDetails();

        Clear();

        Print(0, 0, "Connected devices");

        uint32_t offset = 13;
        for (const auto &client : clients)
        {
            Print(0, offset, client.IPAddress);
            offset += 10;

            Print(0, offset, client.MacAddress);
            offset += 10;
        }

        Refresh();
    }

    void NextMenu()
    {
        using Menus = Configuration::Menu::Menus;
        using Sensors = Configuration::Sensor::Sensors;

        if (Storage::GetConfigMode())
        {
            switch (currentMenu)
            {
            case Menus::Config:
                currentMenu = Menus::ConfigClients;
                return;

            case Menus::ConfigClients:
                currentMenu = Menus::Failsafe;
                return;

            case Menus::Failsafe:
                if (Configuration::Notification::Get(Configuration::Notification::ConfigSet))
                {
                    if (WiFi::IsConnected())
                        currentMenu = Menus::ConfigConnected;
                    else
                        currentMenu = Menus::ConfigConnecting;
                    return;
                }

                currentMenu = Menus::Config;
                return;

            case Menus::ConfigConnecting:
            case Menus::ConfigConnected:
                currentMenu = Menus::Failsafe;
                return;

            default:
                return;
            }
        }

        else
        {
            switch (currentMenu)
            {
            case Menus::Failsafe:
            case Menus::Reset:
                currentMenu = Menus::Main;
                return;

            default:
                for (int i = currentMenu + 1; i < Menus::Failsafe; i++)
                {
                    if (Storage::GetSensorState((Sensors)i))
                    {
                        if (Climate::IsOK() && (i >= Sensors::Temperature && i <= Sensors::Altitude))
                        {
                            currentMenu = (Menus)i;
                            return;
                        }

                        if (Mic::IsOK() && (i >= Sensors::Loudness || i <= Sensors::Recording))
                        {
                            currentMenu = (Menus)i;
                            return;
                        }
                    }
                }

                currentMenu = Menus::Failsafe;
            }
        }
    }

    bool IsOK() { return isOK; }
    Configuration::Menu::Menus GetMenu() { return currentMenu; }
    void SetMenu(Configuration::Menu::Menus menu)
    {
        currentMenu = menu;
        ResetScreenSaver();
    }
}
