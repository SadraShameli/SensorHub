#pragma once

#include <cstdint>
#include <string>

#include "core/Service.h"

namespace Network {

extern const Kernel::Service kService;

enum class ConfigState : uint8_t {
    Idle,
    Saving,
    Connecting,
    ProbingBackend,
    Success,
    Failed,
};

struct ConfigStatusSnapshot {
    ConfigState state;
    std::string message;
};

void Init();
void Update();
void UpdateConfig();
void Reset();

void NotifyConfigSet();
bool IsConfigSubmitted();

void SetConfigStatus(ConfigState state, const std::string& message);
ConfigStatusSnapshot GetConfigStatus();
const char* ConfigStateName(ConfigState state);

}