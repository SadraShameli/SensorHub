#include "sensors/SensorRegistry.h"

#include "esp_log.h"

namespace Sensors {

static const char* TAG = "SensorReg";

SensorRegistry& SensorRegistry::Instance() {
    static SensorRegistry s_instance;
    return s_instance;
}

void SensorRegistry::Register(ISensor* sensor) {
    if (sensor == nullptr) {
        return;
    }
    if (m_count >= kCapacity) {
        ESP_LOGE(TAG,
                 "Registry full; dropping sensor id=%u",
                 (unsigned)sensor->Id());
        return;
    }

    for (size_t i = 0; i < m_count; ++i) {
        if (m_sensors[i]->Id() == sensor->Id()) {
            ESP_LOGW(TAG,
                     "Sensor id=%u already registered; skipping",
                     (unsigned)sensor->Id());
            return;
        }
    }
    m_sensors[m_count++] = sensor;
    ESP_LOGI(TAG,
             "Registered %s (id=%u)",
             sensor->Name(),
             (unsigned)sensor->Id());
}

ISensor* SensorRegistry::ById(uint8_t id) const {
    for (size_t i = 0; i < m_count; ++i) {
        if (m_sensors[i]->Id() == id) {
            return m_sensors[i];
        }
    }
    return nullptr;
}

}
