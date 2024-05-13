#pragma once
#include "Backend.h"

class Output
{
public:
    static void Init();
    static void Update();
    static void Toggle(DeviceConfig::Outputs, bool);
    static void Blink(DeviceConfig::Outputs, clock_t = 50, bool = false);
    static void SetContinuity(DeviceConfig::Outputs, bool);
};
