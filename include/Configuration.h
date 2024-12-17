#pragma once

#include <cstdint>

/**
 * @namespace
 * @brief A namespace for managing configuration.
 */
namespace Configuration {

/**
 * @namespace
 * @brief A namespace for managing Wi-Fi.
 */
namespace WiFi {

static const char SSID[] = "Unit", Password[] = "";

}

/**
 * @namespace
 * @brief A namespace for managing sensors.
 */
namespace Sensor {

/**
 * @enum
 * @brief An enum containing the possible sensors.
 */
enum Sensors {
    Temperature = 1,
    Humidity,
    AirPressure,
    GasResistance,
    Altitude,
    Loudness,
    Recording,
    RPM,
    SensorCount
};

};  // namespace Sensor

/**
 * @namespace
 * @brief A namespace for managing the menu.
 */
namespace Menu {

/**
 * @enum
 * @brief An enum containing the possible menus.
 */
enum Menus {
    Main,
    Temperature,
    Humidity,
    AirPressure,
    GasResistance,
    Altitude,
    Loudness,
    Recording,
    RPM,
    Failsafe,
    Config,
    ConfigClients,
    ConfigConnecting,
    ConfigConnected,
    Reset,
};

};  // namespace Menu

/**
 * @namespace
 * @brief A namespace for managing notifications.
 */
namespace Notification {

/**
 * @enum
 * @brief An enum containing the possible notifications.
 */
enum Notifications {
    NewFailsafe = 1,
    ConfigSet,
};

inline static uint32_t Values = 0;

/**
 * @brief Gets the specified notification.
 *
 * @param value The notification to get.
 *
 * @return `true` if the notification is set, `false` otherwise.
 */
inline bool Get(Notifications value) {
    return Values & value;
}

/**
 * @brief Sets the specified notification.
 *
 * @param value The notification to set.
 */
inline void Set(Notifications value) {
    Values |= value;
}

};  // namespace Notification
}  // namespace Configuration
