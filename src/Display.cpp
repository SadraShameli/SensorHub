#include "Display.h"

#include <cstdint>

#include <cstdio>

#include "Configuration.h"
#include "Gui.h"
#include "Network.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"
#include "ssd1306.h"

namespace Display {
namespace Constants {

static const uint32_t LogoDurationMs = 1000;
static const int64_t ScreenSaverDurationUs = 60LL * 1000LL * 1000LL;

};

static const char* TAG = "Display";

static ssd1306_handle_t dev = nullptr;
static Configuration::Menu::Menus currentMenu = Configuration::Menu::Main;
static int64_t prevActivityUs = 0;
static bool displayOff = false, isOK = false;

void Init() {
    ESP_LOGI(TAG, "Initializing");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master =
            {
                .clk_speed = 1000000,
            },
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    dev = ssd1306_create(I2C_NUM_0, 0x3c);
    if (dev == nullptr) {
        ESP_LOGI(TAG, "No display detected");

        vTaskDelete(nullptr);
    }

    isOK = true;

    Clear();

    Print(35, 20, ">_sadra");
    Print(30, 40, "Sensor Hub");
    Refresh();
    vTaskDelay(pdMS_TO_TICKS(Constants::LogoDurationMs));

    Clear();

    if (Storage::GetConfigMode()) {
        currentMenu = Configuration::Menu::Config;
    }
}

void Update() {
    const int64_t nowUs = esp_timer_get_time();

    if ((nowUs - prevActivityUs) > Constants::ScreenSaverDurationUs) {
        prevActivityUs = nowUs;
        displayOff = true;

        ssd1306_display_off(dev);
        Gui::Pause();
    }
}

bool IsOK() {
    return isOK;
}

void Clear() {
    ssd1306_clear_screen(dev, 0x00);
}

void Refresh() {
    ssd1306_refresh_gram(dev);
}

void ResetScreenSaver() {
    if (!displayOff) {
        return;
    }

    prevActivityUs = esp_timer_get_time();
    displayOff = false;

    ssd1306_display_on(dev);
    Gui::Resume();
}

void Print(uint32_t x, uint32_t y, const char* text, uint32_t size) {
    ssd1306_draw_string(dev,
                        (uint8_t)x,
                        (uint8_t)y,
                        (const uint8_t*)text,
                        (uint8_t)size,
                        1);
}

void PrintText(const char* header, const char* message) {
    Clear();

    Print(0, 0, header);
    Print(0, 16, message);

    Refresh();
}

void PrintLines(const char* line1, const char* line2, const char* line3,
                const char* line4) {
    Clear();

    Print(0, 0, line1);
    Print(0, 16, line2);
    Print(0, 32, line3);
    Print(0, 48, line4);

    Refresh();
}

void PrintMain() {
    char buff[32] = {0};
    Clear();

    Print(0, 0, Storage::GetDeviceName().c_str());

    if (WiFi::IsConnected()) {
        char ipLine[32];
        snprintf(ipLine,
                 sizeof(ipLine),
                 "IP: %s",
                 WiFi::GetIPStation().c_str());
        Print(0, 13, ipLine);
    } else {
        Print(0, 13, "Connecting to WiFi");
    }

    constexpr uint32_t kRowYs[] = {26, 39, 52};
    uint32_t row = 0;
    const auto& reg = Sensors::SensorRegistry::Instance();
    for (auto it = reg.Begin(); it != reg.End() && row < 3; ++it) {
        const auto* s = *it;
        const auto cfgId = static_cast<Configuration::Sensor::Sensors>(s->Id());

        if (!Storage::GetSensorState(cfgId) || !s->IsOk()) {
            continue;
        }

        snprintf(buff,
                 sizeof(buff),
                 "%s: %d%s",
                 s->Name(),
                 static_cast<int>(s->Snapshot().Current()),
                 s->Unit());
        Print(0, kRowYs[row++], buff);
    }

    Refresh();
}

void PrintSensorMenu(const Sensors::ISensor& sensor) {
    const Reading r = sensor.Snapshot();

    char buff[32] = {0};
    Clear();

    Print(0, 0, sensor.Name());

    snprintf(buff,
             sizeof(buff),
             "%d%s",
             static_cast<int>(r.Current()),
             sensor.Unit());
    Print(0, 16, buff);

    snprintf(buff,
             sizeof(buff),
             "Max: %d%s",
             static_cast<int>(r.Max()),
             sensor.Unit());
    Print(0, 32, buff);

    snprintf(buff,
             sizeof(buff),
             "Min: %d%s",
             static_cast<int>(r.Min()),
             sensor.Unit());
    Print(0, 48, buff);

    Refresh();
}

void PrintWiFiClients() {
    const std::vector<WiFi::ClientDetails>& clients = WiFi::GetClientDetails();

    Clear();

    Print(0, 0, "Connected devices");

    uint32_t offset = 13;
    for (const auto& client : clients) {
        Print(0, offset, client.IPAddress);
        offset += 10;

        Print(0, offset, client.MacAddress);
        offset += 10;
    }

    Refresh();
}

void NextMenu() {
    using Menus = Configuration::Menu::Menus;
    using Sensors = Configuration::Sensor::Sensors;

    if (Storage::GetConfigMode()) {
        switch (currentMenu) {
            case Menus::Config:
                currentMenu = Menus::ConfigClients;
                return;

            case Menus::ConfigClients:
                currentMenu = Menus::Failsafe;
                return;

            case Menus::Failsafe:
                if (Network::IsConfigSubmitted()) {
                    if (WiFi::IsConnected()) {
                        currentMenu = Menus::ConfigConnected;
                    } else {
                        currentMenu = Menus::ConfigConnecting;
                    }

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

    else {
        switch (currentMenu) {
            case Menus::Failsafe:
            case Menus::Reset:
                currentMenu = Menus::Main;
                return;

            default: {

                const auto& reg = ::Sensors::SensorRegistry::Instance();
                for (int i = currentMenu + 1; i < Menus::Failsafe; i++) {
                    const auto cfgId = static_cast<Sensors>(i);
                    if (!Storage::GetSensorState(cfgId)) {
                        continue;
                    }
                    const auto* s = reg.ById(static_cast<uint8_t>(i));
                    if (s && s->IsOk()) {
                        currentMenu = static_cast<Menus>(i);
                        return;
                    }
                }
                currentMenu = Menus::Failsafe;
            }
        }
    }
}

Configuration::Menu::Menus GetMenu() {
    return currentMenu;
}

void SetMenu(Configuration::Menu::Menus menu) {
    currentMenu = menu;

    ResetScreenSaver();
}

}
