#pragma once
#include <ctime>
#include "Backend.h"

class Output
{
public:
    enum Outputs
    {
        LedR = 32,
        LedY = 33,
        LedG = 25
    };

    static void Init();
    static void Update();
    static void Toggle(Outputs, bool);
    static void Blink(Outputs, clock_t = 50, bool = false);
    static void SetContinuity(Outputs, bool);
};