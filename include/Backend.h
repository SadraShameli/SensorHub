#pragma once

#include <string>

#include "HTTP.h"

/**
 * @namespace
 * @brief A namespace for managing backend related functionality.
 */
namespace Backend {

bool CheckResponseFailed(const std::string &, HTTP::Status::StatusCode);
bool SetupConfiguration(const std::string &);
void GetConfiguration();
bool RegisterReadings();

extern std::string DeviceURL, ReadingURL, RecordingURL;

};  // namespace Backend