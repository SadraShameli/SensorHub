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

  Storage::GetSSID(Backend::SSID);
  Storage::GetPassword(Backend::Password);
  Storage::GetDeviceId(Backend::DeviceId);
  Storage::GetDeviceName(Backend::DeviceName);
  Storage::GetAuthKey(Backend::AuthKey);
  Storage::GetAddress(Backend::Address);
  Storage::GetLoudnessThreshold(Backend::LoudnessThreshold);
  Storage::GetRegisterInterval(Backend::RegisterInterval);

  WiFi::Init();
  WiFi::StartStation();
  HTTP::Init();

  if (Storage::GetEnabledSensors(Backend::SensorTypes::Sound))
  {
    Sound::Init();
  }
}