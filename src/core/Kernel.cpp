#include "core/Kernel.h"

#include "Storage.h"
#include "esp_log.h"

namespace Kernel {

static const char* TAG = "Kernel";

static inline bool ShouldRunInMode(const Service& s, bool cfgMode) {
    const uint8_t requiredFlag = cfgMode ? RunInConfigMode : RunInNormalMode;
    return (s.modes & requiredFlag) != 0;
}

void Boot(const Service* const* services, std::size_t count) {
    ESP_LOGI(TAG, "Booting %u services", static_cast<unsigned>(count));

    bool cfgMode = true;
    bool cfgModeKnown = false;

    for (std::size_t i = 0; i < count; ++i) {
        const Service& s = *services[i];

        if (cfgModeKnown && !ShouldRunInMode(s, cfgMode)) {
            continue;
        }
        if (s.should_start && !s.should_start()) {
            continue;
        }

        if (s.on_init) {
            ESP_LOGI(TAG, "init  %s", s.name);
            s.on_init();
        }

        if (!cfgModeKnown) {
            cfgMode = Storage::GetConfigMode();
            cfgModeKnown = true;
            ESP_LOGI(TAG, "config_mode=%s", cfgMode ? "yes" : "no");
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        const Service& s = *services[i];

        if (!ShouldRunInMode(s, cfgMode))
            continue;
        if (s.should_start && !s.should_start())
            continue;
        if (!s.task_entry)
            continue;

        TaskHandle_t handle = nullptr;
        const BaseType_t rc = xTaskCreate(s.task_entry,
                                          s.name,
                                          s.stack_bytes,
                                          nullptr,
                                          s.priority,
                                          &handle);
        if (rc != pdPASS) {
            ESP_LOGE(TAG,
                     "spawn %s FAILED rc=%d",
                     s.name,
                     static_cast<int>(rc));
            continue;
        }

        if (s.out_handle)
            *s.out_handle = handle;
        ESP_LOGI(TAG,
                 "spawn %s prio=%u stack=%u",
                 s.name,
                 static_cast<unsigned>(s.priority),
                 static_cast<unsigned>(s.stack_bytes));
    }

    ESP_LOGI(TAG, "Boot complete");
}

}
