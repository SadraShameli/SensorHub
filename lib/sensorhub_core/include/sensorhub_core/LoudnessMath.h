#pragma once

#include <cmath>

namespace sensorhub::core {

constexpr float kInmp441SensitivityDb = 26.0f;
constexpr float kInmp441ReferenceSpl = 94.0f;
constexpr float kInmp441OffsetDb = -3.0f;
constexpr float kInmp441FloorDb = 29.0f;
constexpr float kInmp441PeakDb = 116.0f;

inline float FullScaleAmplitude16Bit() {
    constexpr float kFullScale16 = 32767.0f;
    return std::pow(10.0f, -kInmp441SensitivityDb / 20.0f) * kFullScale16;
}

inline float RmsToSpl(float rms, float amplitude) {
    if (rms <= 0.0f || amplitude <= 0.0f) {
        return 0.0f;
    }

    const float spl = 20.0f * std::log10(rms / amplitude) +
                      kInmp441ReferenceSpl + kInmp441OffsetDb;

    if (spl > kInmp441FloorDb && spl < kInmp441PeakDb) {
        return spl;
    }

    return 0.0f;
}

}
