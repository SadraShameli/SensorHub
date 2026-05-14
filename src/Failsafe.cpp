#include "Failsafe.h"

#include <array>

#include "Configuration.h"
#include "Display.h"
#include "Output.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace Failsafe {

static const char* TAG = "Failsafe";
static TaskHandle_t xHandle = nullptr;

static constexpr size_t kCapacity = 24;
static std::array<Failure, kCapacity> failures;
static size_t failureHead = 0;
static size_t failureCount = 0;
static SemaphoreHandle_t failuresMutex = nullptr;

struct ScopedLock {
    ScopedLock() {
        if (failuresMutex) {
            xSemaphoreTake(failuresMutex, portMAX_DELAY);
        }
    }

    ~ScopedLock() {
        if (failuresMutex) {
            xSemaphoreGive(failuresMutex);
        }
    }
};

static size_t TopIndex() {
    return (failureHead + kCapacity - 1) % kCapacity;
}

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    for (;;) {
        Update();
    }

    vTaskDelete(nullptr);
}

static void OnInit() {
    failuresMutex = xSemaphoreCreateMutex();
}

void Init() {
    OnInit();
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 1, &xHandle);
}

const Kernel::Service kService = {
    .name = "Failsafe",
    .modes = Kernel::RunAlways,
    .on_init = &OnInit,
    .task_entry = &vTask,
    .stack_bytes = 4096,
    .priority = tskIDLE_PRIORITY + 1,
    .out_handle = &xHandle,
    .should_start = nullptr,
};

void Update() {
    ESP_LOGI(TAG, "Waiting for failure");

    uint32_t notif = 0;
    xTaskNotifyWait(0,
                    Configuration::Notification::Raw(
                        Configuration::Notification::Bits::NewFailsafe),
                    &notif,
                    portMAX_DELAY);

    {
        ScopedLock lock;
        if (failureCount > 0) {
            const Failure& topFailure = failures[TopIndex()];
            ESP_LOGE(TAG,
                     "%s - %s",
                     topFailure.Caller,
                     topFailure.Message.c_str());
        }
    }

    Output::Blink(Output::LedR, 5000);
    Display::SetMenu(Configuration::Menu::Failsafe);
}

void AddFailure(const char* caller, std::string&& message) {
    {
        ScopedLock lock;

        failures[failureHead] = Failure(caller, std::move(message));
        failureHead = (failureHead + 1) % kCapacity;
        if (failureCount < kCapacity) {
            ++failureCount;
        }
        ESP_LOGI(TAG, "Pushed failure - count: %u", (unsigned)failureCount);
    }

    xTaskNotify(xHandle,
                Configuration::Notification::Raw(
                    Configuration::Notification::Bits::NewFailsafe),
                eSetBits);
}

void AddFailureDelayed(const char* caller, std::string&& message) {
    AddFailure(caller, std::move(message));

    vTaskDelay(pdMS_TO_TICKS(10000));
}

void PopFailure() {
    ScopedLock lock;

    if (failureCount > 0) {
        failureHead = (failureHead + kCapacity - 1) % kCapacity;
        failures[failureHead].Message.clear();
        failures[failureHead].Caller = nullptr;
        --failureCount;
        ESP_LOGI(TAG, "Popped failure");
    }
}

std::optional<Failure> PeekTopFailure() {
    ScopedLock lock;
    if (failureCount == 0) {
        return std::nullopt;
    }
    return failures[TopIndex()];
}

size_t FailureCount() {
    ScopedLock lock;
    return failureCount;
}

}
