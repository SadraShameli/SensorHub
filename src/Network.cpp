#include "Network.h"

#include <atomic>
#include <cstdint>
#include <mutex>

#include "Backend.h"
#include "Configuration.h"
#include "Display.h"
#include "HTTP.h"
#include "Mic.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Network {

static const char* TAG = "Network";
static TaskHandle_t xHandle = nullptr;

static uint32_t registerInterval = 0;
static std::atomic<bool> s_ConfigSubmitted{false};

static std::mutex s_statusMutex;
static ConfigStatusSnapshot s_status{ConfigState::Idle, ""};

static const uint32_t kStaConnectTimeoutMs = 20000;
static const uint32_t kRebootDelayMs = 3000;

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    if (Storage::GetConfigMode()) {
        WiFi::StartAP();

        for (;;) {
            UpdateConfig();

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    else {
        WiFi::StartStation();

        if (Storage::GetSensorState(Configuration::Sensor::Recording)) {
            vTaskDelete(nullptr);
        }

        HTTP::Init();
        registerInterval = Storage::GetRegisterInterval() * 1000;

        for (;;) {
            Update();

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    vTaskDelete(nullptr);
}

void Init() {
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 5, &xHandle);
}

const Kernel::Service kService = {
    .name = "Network",
    .modes = Kernel::RunAlways,
    .on_init = nullptr,
    .task_entry = &vTask,
    .stack_bytes = 8192,
    .priority = tskIDLE_PRIORITY + 5,
    .out_handle = &xHandle,
    .should_start = nullptr,
};

void Update() {
    vTaskDelay(pdMS_TO_TICKS(registerInterval));

    if (Backend::RegisterReadings()) {
        Mic::ResetValues();

        Output::Blink(Output::LedG, 1000);
    }
}

void UpdateConfig() {
    Output::Blink(Output::LedY, 1000, true);

    uint32_t notif = 0;
    xTaskNotifyWait(0,
                    Configuration::Notification::Raw(
                        Configuration::Notification::Bits::ConfigSet),
                    &notif,
                    portMAX_DELAY);

    if (!(notif & Configuration::Notification::Raw(
                      Configuration::Notification::Bits::ConfigSet))) {
        return;
    }

    Display::SetMenu(Configuration::Menu::ConfigConnecting);
    SetConfigStatus(ConfigState::Connecting,
                    "Connecting to WiFi: " + Storage::GetSSID());

    WiFi::StartStation();
    HTTP::Init();

    if (!WiFi::WaitForConnection(kStaConnectTimeoutMs)) {
        const char* reason =
            WiFi::DisconnectReasonString(WiFi::GetLastDisconnectReason());
        ESP_LOGW(TAG, "STA connection failed: %s", reason);
        SetConfigStatus(ConfigState::Failed, reason);
        s_ConfigSubmitted.store(false, std::memory_order_release);
        return;
    }

    Display::SetMenu(Configuration::Menu::ConfigConnected);
    SetConfigStatus(ConfigState::ProbingBackend,
                    "Verifying backend at " + Storage::GetAddress());

    Backend::ProbeResult result = Backend::ProbeAndStoreConfiguration();
    if (!result.ok) {
        ESP_LOGW(TAG, "Backend probe failed: %s", result.error.c_str());
        SetConfigStatus(ConfigState::Failed, result.error);
        s_ConfigSubmitted.store(false, std::memory_order_release);
        return;
    }

    SetConfigStatus(ConfigState::Success, "Configuration saved, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(kRebootDelayMs));
    esp_restart();
}

void Reset() {
    vTaskDelete(xHandle);
    Init();
}

void NotifyConfigSet() {
    s_ConfigSubmitted.store(true, std::memory_order_release);
    SetConfigStatus(ConfigState::Saving, "Configuration accepted, applying...");
    xTaskNotify(xHandle,
                Configuration::Notification::Raw(
                    Configuration::Notification::Bits::ConfigSet),
                eSetBits);
}

bool IsConfigSubmitted() {
    return s_ConfigSubmitted.load(std::memory_order_acquire);
}

void SetConfigStatus(ConfigState state, const std::string& message) {
    std::lock_guard<std::mutex> lock(s_statusMutex);
    s_status.state = state;
    s_status.message = message;
}

ConfigStatusSnapshot GetConfigStatus() {
    std::lock_guard<std::mutex> lock(s_statusMutex);
    return s_status;
}

const char* ConfigStateName(ConfigState state) {
    switch (state) {
        case ConfigState::Idle:
            return "idle";
        case ConfigState::Saving:
            return "saving";
        case ConfigState::Connecting:
            return "connecting";
        case ConfigState::ProbingBackend:
            return "probing";
        case ConfigState::Success:
            return "success";
        case ConfigState::Failed:
            return "failed";
    }
    return "unknown";
}

}