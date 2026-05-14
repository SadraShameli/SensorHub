#pragma once

#include <cstdint>

namespace Configuration {
namespace Sensor {

enum Sensors {
    Temperature = 1,
    Humidity,
    AirPressure,
    GasResistance,
    Altitude,
    Loudness,
    Recording,
    RPM,
    SensorCount,
};

}
}
