#pragma once

namespace RPM
{
    void Init();
    void Update();

    unsigned long GetRPM();
    unsigned long GetMaxRPM();
    unsigned long GetAvgRPM();
    void ResetValues();
};