#pragma once

#include "Configuration.h"
#include "Definitions.h"

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