#pragma once
#include <string>

namespace DeviceConfig
{
    namespace WiFi
    {
        static const char SSID[] = "Unit", Password[] = "";
        static const int ConnectionRetries = 10;
        extern const char ServerCrt[] asm("_binary_cer_crt_start");
    }

    namespace Tasks
    {
        namespace Notifications
        {
            enum NotificationStates
            {
                ConfigSet = 1,
                NewFailsafe
            };

            inline uint32_t Notification = 0;
        };
    }
}

class Backend
{
public:
    enum DeviceTypes
    {
        Recording = 1,
        Reading,
    };

    enum SensorTypes
    {
        Temperature = 1,
        Humidity,
        AirPressure,
        GasResistance,
        Altitude,
        Sound,
        RPM,
        SensorCount
    };

    enum Menus
    {
        Config = SensorCount,
        ConfigClients,
        Failsafe,
        Main,
        Reset
    };

public:
    static bool CheckResponseFailed(std::string &, int);
    static bool SetupConfiguration(std::string &);
    static void GetConfiguration();
    static bool RegisterReadings();

public:
    inline static std::string DeviceURL = "device/", ReadingURL = "reading/", RecordingURL = "recording/";
};