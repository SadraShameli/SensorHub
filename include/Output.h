#pragma once
#include <ctime>

namespace Output
{
    enum Outputs
    {
        LedR = 32,
        LedY = 33,
        LedG = 25
    };

    void Init();
    void Update();

    void Toggle(Outputs, bool);
    void Blink(Outputs, clock_t = 50, bool = false);
    void SetContinuity(Outputs, bool);
};