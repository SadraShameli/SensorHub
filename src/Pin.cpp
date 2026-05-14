#include "Pin.h"

#include <cstdint>
#include <vector>

#include "Configuration.h"
#include "Display.h"
#include "Failsafe.h"
#include "Input.h"
#include "Output.h"
#include "Storage.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"

namespace Pin {

static const char* TAG = "Pin";
static TaskHandle_t xHandle = nullptr;

static bool resetCanceled = false;

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    Input::Init();
    Output::Init();

    for (;;) {
        Input::Update();
        Output::Update();
        Update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(nullptr);
}

void Init() {
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
}

const Kernel::Service kService = {
    .name = "Pin",
    .modes = Kernel::RunAlways,
    .on_init = nullptr,
    .task_entry = &vTask,
    .stack_bytes = 4096,
    .priority = tskIDLE_PRIORITY,
    .out_handle = &xHandle,
    .should_start = nullptr,
};

void Update() {
    using Menus = Configuration::Menu::Menus;

    if (Input::GetPinState(Input::Inputs::Up)) {
        Output::Blink(Output::LedY);
        Display::ResetScreenSaver();

        if (!Storage::GetConfigMode()) {
            constexpr int64_t kResetWindowUs = 10LL * 1000LL * 1000LL;
            const int64_t resetStartUs = esp_timer_get_time();

            while (!resetCanceled &&
                   (esp_timer_get_time() - resetStartUs) < kResetWindowUs) {
                Display::SetMenu(Menus::Reset);
                Input::Update();
                Output::Update();

                if (Input::GetPinState(Input::Up)) {
                    resetCanceled = true;
                    return;
                }

                if (Input::GetPinState(Input::Inputs::Down)) {
                    Storage::Reset();
                    esp_restart();
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (!resetCanceled) {
                Display::SetMenu(Menus::Main);
                resetCanceled = true;
            }
        }

        Display::NextMenu();
    }

    else if (Input::GetPinState(Input::Inputs::Down)) {
        Output::Blink(Output::LedY);
        Display::ResetScreenSaver();

        if (Storage::GetConfigMode()) {
            if (Display::GetMenu() != Menus::Failsafe) {
                esp_restart();
            }

            Failsafe::PopFailure();
        }

        const auto menu = Display::GetMenu();

        if (menu == Menus::Failsafe) {
            Failsafe::PopFailure();
            return;
        }

        auto* sensor = ::Sensors::SensorRegistry::Instance().ById(
            static_cast<uint8_t>(menu));
        if (sensor) {
            sensor->ResetMinMax();
        }
    }
}

}