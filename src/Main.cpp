#include "Pin.h"
#include "Failsafe.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Sound.h"

extern "C" void app_main()
{
  Failsafe::Init();
  Storage::Init();
  Pin::Init();

  if (Storage::GetConfigMode())
  {
    Network::Init();
    return;
  }

  WiFi::Init();
  WiFi::StartStation();
  HTTP::Init();

  if (Storage::GetDeviceType() == Backend::DeviceTypes::Recording || Storage::GetEnabledSensors(Backend::SensorTypes::Sound))
  {
    Sound::Init();
  }
}