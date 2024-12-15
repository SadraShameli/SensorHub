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
  {
    return;
  }

  using Sensors = Configuration::Sensor::Sensors;

  if (Storage::GetSensorState(Sensors::Loudness) || Storage::GetSensorState(Sensors::Recording))
  {
    Mic::Init();
  }

  if (Storage::GetSensorState(Sensors::Temperature) ||
      Storage::GetSensorState(Sensors::Humidity) ||
      Storage::GetSensorState(Sensors::AirPressure) ||
      Storage::GetSensorState(Sensors::GasResistance) ||
      Storage::GetSensorState(Sensors::Altitude))
  {
    Climate::Init();
  }
}
