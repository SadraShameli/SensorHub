#pragma once

#include <cstdint>

namespace Configuration {
namespace Notification {

enum class Bits : uint32_t {
    NewFailsafe = 1u << 0,
    ConfigSet = 1u << 1,
};

inline constexpr uint32_t Raw(Bits b) {
    return static_cast<uint32_t>(b);
}

}
}
