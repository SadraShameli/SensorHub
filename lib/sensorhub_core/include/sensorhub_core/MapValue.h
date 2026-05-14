#pragma once

namespace sensorhub::core {

template <typename T>
inline long MapValue(T x, T in_min, T in_max, T out_min, T out_max) {
    const T run = in_max - in_min;
    if (run == 0) {
        return -1;
    }
    const T rise = out_max - out_min;
    const T delta = x - in_min;
    return static_cast<long>((delta * rise) / run + out_min);
}

}
