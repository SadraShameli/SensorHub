#pragma once

namespace Configuration
{
    namespace WiFi
    {
        static const char SSID[] = "Unit", Password[] = "";
        extern const char ServerCrt[] asm("_binary_cer_crt_start");
        static const int ConnectionRetries = 10;
    }

    class Sensors
    {
    public:
        enum Sensor
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

    class Menus
    {
    public:
        enum Menu
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

    class Notifications
    {
    public:
        enum Notification
        {
            NewFailsafe = 1,
            ConfigSet,
        };

        static bool Get(Notification notification) { return Notification & notification; }
        static void Set(uint32_t notification) { Notification |= notification; }
        inline static uint32_t Notification = 0;
    };
}
