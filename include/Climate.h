#pragma once
#include "Definitions.h"
#include "Configuration.h"

namespace Climate
{
    void Init();
    void Update();

    void ResetValues(Configuration::Sensor::Sensors);

    bool IsOK();
    const Reading &GetTemperature();
    const Reading &GetHumidity();
    const Reading &GetAirPressure();
    const Reading &GetGasResistance();
    const Reading &GetAltitude();
};