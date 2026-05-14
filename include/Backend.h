#pragma once

#include <string>

#include "HTTP.h"

namespace Backend {

struct ProbeResult {
    bool ok;
    std::string error;
};

bool CheckResponseFailed(const std::string &, HTTP::Status::StatusCode);
bool SetupConfiguration(const std::string &);
ProbeResult ProbeAndStoreConfiguration();
bool RegisterReadings();

extern std::string DeviceURL, ReadingURL, RecordingURL;

};