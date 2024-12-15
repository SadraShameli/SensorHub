#include "Climate.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "Gui.h"
#include "Mic.h"
#include "Network.h"
#include "Pin.h"
#include "Storage.h"
#include "WiFi.h"

/**
 * @brief Main application entry point.
 *
 * This function initializes various subsystems and sensors required for the
 * application. It performs the following initialization steps:
 *
 * 1. `Failsafe mechanism`
 *
 * 2. `Storage system`
 *
 * 3. `Pin configuration`
 *
 * 4. `Graphical user interface`
 *
 * 5. `WiFi subsystem`
 *
 * 6. `Network subsystem`
 *
 * If the system is in configuration mode, the function returns early.
 *
 * Depending on the sensor states stored in the configuration, it initializes
 * the microphone and climate sensors as needed:
 *
 * - If either the Loudness or Recording sensor is enabled, the microphone is
 * initialized.
 *
 * - If any of the Temperature, Humidity, Air Pressure, Gas Resistance, or
 * Altitude sensors are enabled, the climate sensors are initialized.
 */
extern "C" void app_main() {
    UNIT_TIMER("Boot");

    Failsafe::Init();
    Storage::Init();
    Pin::Init();
    Gui::Init();
    WiFi::Init();
    Network::Init();

    if (Storage::GetConfigMode()) {
        return;
    }

    using Sensors = Configuration::Sensor::Sensors;

    if (Storage::GetSensorState(Sensors::Loudness) ||
        Storage::GetSensorState(Sensors::Recording)) {
        Mic::Init();
    }

    if (Storage::GetSensorState(Sensors::Temperature) ||
        Storage::GetSensorState(Sensors::Humidity) ||
        Storage::GetSensorState(Sensors::AirPressure) ||
        Storage::GetSensorState(Sensors::GasResistance) ||
        Storage::GetSensorState(Sensors::Altitude)) {
        Climate::Init();
    }
}
