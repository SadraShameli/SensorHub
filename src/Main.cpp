#include "Definitions.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Pin.h"
#include "Gui.h"
#include "WiFi.h"
#include "Network.h"
#include "Sound.h"
#include "Climate.h"

extern "C" void app_main()
{
  UNIT_TIMER("Boot");

  Failsafe::Init();
  Storage::Init();
  Pin::Init();
  Gui::Init();
  WiFi::Init();
  Network::Init();

  if (Storage::GetConfigMode())
    return;

  using Sensors = Configuration::Sensor::Sensors;

  if (Storage::GetSensorState(Sensors::Loudness) || Storage::GetSensorState(Sensors::Recording))
    Sound::Init();

  if (Storage::GetSensorState(Sensors::Temperature) ||
      Storage::GetSensorState(Sensors::Humidity) ||
      Storage::GetSensorState(Sensors::AirPressure) ||
      Storage::GetSensorState(Sensors::GasResistance) ||
      Storage::GetSensorState(Sensors::Altitude))
    Climate::Init();
}
