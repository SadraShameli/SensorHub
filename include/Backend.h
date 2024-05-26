#pragma once
#include <string>

class Backend
{
public:
    static bool CheckResponseFailed(std::string &, int);
    static bool SetupConfiguration(std::string &);
    static void GetConfiguration();
    static bool RegisterReadings();

public:
    inline static std::string DeviceURL = "device/", ReadingURL = "reading/", RecordingURL = "recording/";
};