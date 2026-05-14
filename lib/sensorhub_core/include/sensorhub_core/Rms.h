#pragma once

#include <cmath>
#include <cstdint>

namespace sensorhub::core {

template <typename T>
inline float CalculateRms(const T* input, uint32_t size) {
    if (size == 0) {
        return 0.0f;
    }

    float sumSquares = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        const float sample = static_cast<float>(input[i]);
        sumSquares += sample * sample;
    }

    return std::sqrt(sumSquares / static_cast<float>(size));
}

}
