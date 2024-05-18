#include "esp_log.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Pin.h"
#include "WiFi.h"
#include "Network.h"
#include "Climate.h"
#include "Sound.h"

extern "C" void app_main()
{
  UNIT_TIMER("Main");

  Failsafe::Init();
  Storage::Init();
  WiFi::Init();
  Pin::Init();

  if (Storage::GetConfigMode())
  {
    Network::Init();
    return;
  }

  if (Storage::GetEnabledSensors(Backend::SensorTypes::Recording))
  {
    WiFi::StartStation();
    Sound::Init();
    return;
  }

  Network::Init();
  Climate::Init();
  Sound::Init();
}
