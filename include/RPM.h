#pragma once

class RPM
{
public:
    static void Init();
    static void Update();

    static unsigned long GetRPM();
    static unsigned long GetMaxRPM();
    static unsigned long GetAvgRPM();
    static void ResetValues();
};