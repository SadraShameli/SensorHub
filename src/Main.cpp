#include "esp_log.h"
#include "Definitions.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Pin.h"
#include "WiFi.h"
#include "Network.h"
#include "Gui.h"
#include "Climate.h"
#include "Sound.h"

extern "C" void app_main()
{
  UNIT_TIMER("Boot");

  Failsafe::Init();
  Storage::Init();
  WiFi::Init();
  Pin::Init();

  if (!Storage::GetConfigMode() && Storage::GetSensorState(Configuration::Sensors::Recording))
  {
    WiFi::StartStation();
    Sound::Init();
    return;
  }

  Gui::Init();

  if (Storage::GetConfigMode())
  {
    Network::Init();
    return;
  }

  Network::Init();
  Climate::Init();
  Sound::Init();
}
