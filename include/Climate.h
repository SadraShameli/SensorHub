#pragma once

#include "Configuration.h"
#include "Definitions.h"

/**
 * @namespace
 * @brief A namespace for managing climate data.
 */
namespace Climate {

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

};  // namespace Climate