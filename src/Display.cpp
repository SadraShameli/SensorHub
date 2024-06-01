#include <ctime>
#include "esp_log.h"
#include "ssd1306.h"
#include "Configuration.h"
#include "Display.h"
#include "Gui.h"
#include "Storage.h"
#include "Climate.h"
#include "Sound.h"

static const char *TAG = "Display";
static ssd1306_handle_t dev = nullptr;

void Display::Init()
{
    ESP_LOGI(TAG, "Initializing task");

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

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    dev = ssd1306_create(I2C_NUM_0, 0x3c);
    Clear();

    Print(35, 20, ">_sadra.");
    Print(30, 40, "Sensor Hub");
    Refresh();

    vTaskDelay(pdMS_TO_TICKS(LogoDuration));
    Clear();

    if (Storage::GetConfigMode())
        Display::SetMenu(Configuration::Menus::Config);
}

void Display::Update()
{
    m_CurrentTime = clock();
    if ((m_CurrentTime - m_PrevTime) > ScreenSaverDuration)
    {
        m_PrevTime = m_CurrentTime;
        ssd1306_display_off(dev);
        Gui::Pause();
    }
}

void Display::Clear()
{
    ssd1306_clear_screen(dev, 0x00);
}

void Display::Refresh()
{
    ssd1306_refresh_gram(dev);
}

void Display::ResetScreenSaver()
{
    if (Storage::GetSensorState(Configuration::Sensors::Recording))
        return;

    m_PrevTime = clock();
    ssd1306_display_on(dev);
    Gui::Resume();
}

void Display::Print(uint8_t x, uint8_t y, const char *text, uint8_t size)
{
    ssd1306_draw_string(dev, x, y, (const uint8_t *)text, size, 1);
}

void Display::NextMenu()
{
    if (Storage::GetSensorState(Configuration::Sensors::Recording))
        return;

    using Menus = Configuration::Menus;

    if (Storage::GetConfigMode())
    {
        switch (m_CurrentMenu)
        {
        case Menus::Config:
            m_CurrentMenu = Menus::ConfigClients;
            return;

        case Menus::ConfigClients:
            m_CurrentMenu = Menus::Failsafe;
            return;

        case Menus::Failsafe:
            if (Configuration::Notifications::Get(Configuration::Notifications::ConfigSet))
            {
                if (WiFi::IsConnected())
                    m_CurrentMenu = Menus::ConfigConnected;
                else
                    m_CurrentMenu = Menus::ConfigConnecting;
                return;
            }

            m_CurrentMenu = Menus::Config;
            return;

        case Menus::ConfigConnecting:
        case Menus::ConfigConnected:
            m_CurrentMenu = Menus::Failsafe;
            return;

        default:
            return;
        }
    }

    else
    {
        switch (m_CurrentMenu)
        {
        case Menus::Failsafe:
        case Menus::Reset:
            m_CurrentMenu = Menus::Main;
            return;

        default:
            for (int i = m_CurrentMenu + 1; i < Menus::Failsafe; i++)
            {
                if (Storage::GetSensorState((Configuration::Sensors::Sensor)i))
                {
                    if (i >= Configuration::Sensors::Temperature && i <= Configuration::Sensors::Altitude && Climate::IsOK())
                    {
                        m_CurrentMenu = (Menus::Menu)i;
                        return;
                    }

                    if (i == Configuration::Sensors::Loudness && Sound::IsOK())
                    {
                        m_CurrentMenu = (Menus::Menu)i;
                        return;
                    }
                }
            }

            m_CurrentMenu = Menus::Failsafe;
        }
    }
}

void Display::PrintText(const char *header, const char *message)
{
    Clear();

    Print(0, 0, header);
    Print(0, 16, message);

    Refresh();
}

void Display::PrintLines(const char *line1, const char *line2, const char *line3, const char *line4)
{
    Clear();

    Print(0, 0, line1);
    Print(0, 16, line2);
    Print(0, 32, line3);
    Print(0, 48, line4);

    Refresh();
}

void Display::PrintMain()
{
    const std::string &deviceName = Storage::GetDeviceName(), &ip = WiFi::GetIPStation();
    const Reading &temperature = Climate::GetTemperature(), &humidity = Climate::GetHumidity(), &loudness = Sound::GetLoudness();

    char buff[32] = {};
    Clear();

    Print(0, 0, deviceName.c_str());

    if (Climate::IsOK())
    {
        sprintf(buff, "Temperature: %dc", (int)temperature.Current());
        Print(0, 13, buff);

        sprintf(buff, "Humidity: %d%%", (int)humidity.Current());
        Print(0, 26, buff);
    }

    if (Sound::IsOK())
    {
        sprintf(buff, "Loudness: %ddB", (int)loudness.Current());
        Print(0, 39, buff);
    }

    if (WiFi::IsConnected())
        Print(0, 52, ("IP:" + ip).c_str());
    else
        Print(0, 52, "Connecting to WiFi");

    Refresh();
}

void Display::PrintTemperature()
{
    const Reading &reading = Climate::GetTemperature();

    char buff[32] = {};
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

void Display::PrintHumidity()
{
    const Reading &reading = Climate::GetHumidity();

    char buff[32] = {};
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

void Display::PrintAirPressure()
{
    const Reading &reading = Climate::GetAirPressure();

    char buff[32] = {};
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

void Display::PrintGasResistance()
{
    const Reading &reading = Climate::GetGasResistance();

    char buff[32] = {};
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

void Display::PrintAltitude()
{
    const Reading &reading = Climate::GetAltitude();

    char buff[32] = {};
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

void Display::PrintLoudness()
{
    const Reading &reading = Sound::GetLoudness();

    char buff[32] = {};
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

// void Display::PrintRPM()
// {
//     const Reading &reading = RPM::GetRPM();

//     char buff[32] = {};
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

void Display::PrintWiFiClients()
{
    const std::vector<WiFi::ClientDetails> &clients = WiFi::GetClientDetails();

    Clear();

    Print(0, 0, "Connected devices");

    int offset = 13;
    for (auto &client : clients)
    {
        Print(0, offset, client.IPAddress);
        offset += 10;

        Print(0, offset, client.MacAddress);
        offset += 10;
    }

    Refresh();
}