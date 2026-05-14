#pragma once

#include "Configuration.h"
#include "Definitions.h"
#include "core/Service.h"

namespace Climate {

extern const Kernel::Service kService;

void Init();
void Update();

void ResetValues(Configuration::Sensor::Sensors);
float calculateAltitude(float, float, float);

bool IsOK();
const Reading& GetTemperature();
const Reading& GetHumidity();
const Reading& GetAirPressure();
const Reading& GetGasResistance();
const Reading& GetAltitude();

};