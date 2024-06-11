#pragma once

namespace Configuration
{
    namespace WiFi
    {
        static const char SSID[] = "Unit", Password[] = "";
    }

    namespace Sensor
    {
        enum Sensors
        {
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
    };

    namespace Menu
    {
        enum Menus
        {
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
    };

    namespace Notification
    {
        enum Notifications
        {
            NewFailsafe = 1,
            ConfigSet,
        };

        inline static uint32_t Values = 0;

        inline bool Get(Notifications value) { return Values & value; }
        inline void Set(Notifications value) { Values |= value; }
    };
}
