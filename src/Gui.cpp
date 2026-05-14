#include "Gui.h"

#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Network.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"

namespace Gui {

static const char* TAG = "Gui";
static TaskHandle_t xHandle = nullptr;

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    Display::Init();

    for (;;) {
        Display::Update();
        Update();

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    vTaskDelete(nullptr);
}

void Init() {
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 3, &xHandle);
}

const Kernel::Service kService = {
    .name = "Gui",
    .modes = Kernel::RunAlways,
    .on_init = nullptr,
    .task_entry = &vTask,
    .stack_bytes = 8192,
    .priority = tskIDLE_PRIORITY + 3,
    .out_handle = &xHandle,
    .should_start = nullptr,
};

void Update() {
    using Menus = Configuration::Menu::Menus;

    switch (Display::GetMenu()) {
        case Menus::Main:
            Display::PrintMain();
            break;

        case Menus::Failsafe: {
            const auto failure = Failsafe::PeekTopFailure();

            if (!failure) {
                Display::PrintText("Failsafe",
                                   "There are currently  no failures.");
            }

            else {
                char header[40];
                snprintf(header,
                         sizeof(header),
                         "Failsafe: %s",
                         failure->Caller);
                Display::PrintText(header, failure->Message.c_str());
            }

            break;
        }

        case Menus::Config:
            if (!Network::IsConfigSubmitted()) {
                char ssid[48], pass[32], ip[32];
                snprintf(ssid,
                         sizeof(ssid),
                         "SSID: %s",
                         Configuration::WiFi::SSID);
                snprintf(pass,
                         sizeof(pass),
                         "Pass: %s",
                         Storage::GetApPassword().c_str());
                snprintf(ip, sizeof(ip), "IP:   %s", WiFi::GetIPAP().c_str());
                Display::PrintLines(ssid, pass, ip, "");
            }

            break;

        case Menus::ConfigConnecting: {
            char line[48];
            snprintf(line,
                     sizeof(line),
                     "Connecting to %s",
                     Storage::GetSSID().c_str());
            Display::PrintLines("Configuration", line, "", "");
            break;
        }

        case Menus::ConfigConnected: {
            char line[48];
            snprintf(line,
                     sizeof(line),
                     "Connected to %s",
                     Storage::GetSSID().c_str());
            Display::PrintLines("Configuration", line, "", "Retrieving data");
            break;
        }

        case Menus::ConfigClients:
            Display::PrintWiFiClients();
            break;

        case Menus::Reset:
            Display::PrintText("Configuration",
                               "Press bottom button  to reset device");
            break;

        default: {

            const auto menu = static_cast<uint8_t>(Display::GetMenu());
            const auto* s = Sensors::SensorRegistry::Instance().ById(menu);
            if (s && s->IsOk()) {
                Display::PrintSensorMenu(*s);
            }
            break;
        }
    }
}

void Pause() {
    vTaskSuspend(xHandle);
}

void Resume() {
    vTaskResume(xHandle);
}

}
