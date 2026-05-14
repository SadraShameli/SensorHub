#pragma once

namespace sensorhub::core {

template <typename T>
inline void RemoveTrailingWhitespace(T& s) {
    const auto pos = s.find_last_not_of(" \n\r\t");
    if (pos == T::npos) {
        s.clear();
    } else {
        s.erase(pos + 1);
    }
}

}
