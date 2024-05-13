#pragma once
#include "Backend.h"

class Input
{
public:
    static void Init();
    static void Update();
    static bool GetPinState(DeviceConfig::Inputs);
};
