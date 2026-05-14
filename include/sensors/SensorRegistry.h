#pragma once

#include <array>
#include <cstdint>

#include "sensors/ISensor.h"

namespace Sensors {

class SensorRegistry {
   public:
    static constexpr size_t kCapacity = 16;

    static SensorRegistry& Instance();

    void Register(ISensor* sensor);

    const ISensor* const* Begin() const { return m_sensors.data(); }

    const ISensor* const* End() const { return m_sensors.data() + m_count; }

    size_t Count() const { return m_count; }

    ISensor* ById(uint8_t id) const;

   private:
    SensorRegistry() = default;
    std::array<ISensor*, kCapacity> m_sensors{};
    size_t m_count = 0;
};

}
