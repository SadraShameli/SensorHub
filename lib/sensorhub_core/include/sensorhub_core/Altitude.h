#pragma once

#include <cmath>

namespace sensorhub::core {

inline float CalculateAltitude(float currentPressure, float seaLevelPressure,
                               float seaLevelTempCelsius) {
    constexpr float kLapseRate = 0.0065f;
    constexpr float kGasConstant = 8.31432f;
    constexpr float kGravity = 9.80665f;
    constexpr float kMolarMassAir = 0.0289644f;

    const float seaLevelTempK = seaLevelTempCelsius + 273.15f;

    return (1.0f - std::pow(currentPressure / seaLevelPressure,
                            (kGasConstant * kLapseRate) /
                                (kGravity * kMolarMassAir))) *
           seaLevelTempK / kLapseRate;
}

}
