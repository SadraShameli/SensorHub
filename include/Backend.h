#pragma once

#include <string>

namespace Backend
{
    bool CheckResponseFailed(const std::string &, int);
    bool SetupConfiguration(const std::string &);
    void GetConfiguration();
    bool RegisterReadings();

    extern std::string DeviceURL, ReadingURL, RecordingURL;
};