#include "ArduinoJson.h"
#include "Definitions.h"
#include "Backend.h"
#include "Failsafe.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Climate.h"
#include "Sound.h"

static const char *TAG = "Backend";

bool Backend::CheckResponseFailed(std::string &payload, int statusCode)
{
    if (Request::StatusOK(statusCode))
    {
        ESP_LOGI(TAG, "Backend response ok - status code: %d", statusCode);
        return false;
    }

    if (payload.empty())
    {
        Failsafe::AddFailure(TAG, "Backend response failed - empty response - status code: " + std::to_string(statusCode));
        return true;
    }

    ESP_LOGI(TAG, "Checking backend response");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok)
    {
        Failsafe::AddFailure(TAG, "Deserializing response failed: " + std::string(error.c_str()) + " - status code: " + std::to_string(statusCode));
        return true;
    }

    const char *errorMsg = doc["error"];

    if (errorMsg)
    {
        Failsafe::AddFailure(TAG, "Backend response failed - status code: " + std::to_string(statusCode) + " - error: " + errorMsg);
    }

    return true;
}

bool Backend::SetupConfiguration(std::string &payload)
{
    ESP_LOGI(TAG, "Setting up configuration - payload: %s", payload.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok)
    {
        Failsafe::AddFailure(TAG, "Deserializing setup configuration failed: " + std::string(error.c_str()));
        return false;
    }

    Storage::SetSSID(doc["ssid"].as<const char *>());
    Storage::SetPassword(doc["pass"].as<const char *>());
    Storage::SetDeviceId(doc["device_id"]);
    std::string address = doc["address"].as<const char *>();

    if (Storage::GetSSID().length() > 32 || Storage::GetSSID().empty())
    {
        Failsafe::AddFailure(TAG, "SSID too long or too short");
        return false;
    }

    if ((Storage::GetPassword().length() < 8 && Storage::GetPassword().length() > 64) || Storage::GetPassword().empty())
    {
        Failsafe::AddFailure(TAG, "Password too long or too short");
        return false;
    }

    if (!Storage::GetDeviceId())
    {
        Failsafe::AddFailure(TAG, "Device Id can't be empty");
        return false;
    }

    if (address.empty())
    {
        Failsafe::AddFailure(TAG, "Address can't be empty");
        return false;
    }

    Helpers::RemoveWhiteSpace(address);
    if (address.back() != '/')
    {
        address += "/";
    }

    Storage::SetAddress(address);
    Network::NotifyConfigSet();

    return true;
}

void Backend::GetConfiguration()
{
    ESP_LOGI(TAG, "Getting configuration");

    Request request = Request(Storage::GetAddress() + DeviceURL + std::to_string(Storage::GetDeviceId()));

    if (request.GET())
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, request.GetResponse());

        if (error != DeserializationError::Ok)
        {
            Failsafe::AddFailure(TAG, "Deserializing configuration failed: " + std::string(error.c_str()));
            return;
        }

        Storage::SetDeviceName(doc["name"].as<const char *>());
        Storage::SetDeviceType(doc["type_id"]);
        Storage::SetDeviceId(doc["device_id"]);
        Storage::SetRegisterInterval(doc["register_interval"]);
        Storage::SetLoudnessThreshold(doc["loudness_threshold"]);

        JsonArray sensors = doc["sensors"].as<JsonArray>();
        for (JsonVariant sensor : sensors)
            Storage::SetEnabledSensors(sensor.as<Backend::SensorTypes>(), true);

        Storage::SetConfigMode(false);
        Storage::Commit();
        esp_restart();
    }

    ESP_LOGE(TAG, "Getting configuration failed");
}

bool Backend::RegisterReadings()
{
    if (WiFi::IsConnected())
    {
        ESP_LOGI(TAG, "Registering readings");

        JsonDocument doc;
        doc["device_id"] = Storage::GetDeviceId();

        JsonObject sensors = doc["sensors"].to<JsonObject>();

        if (Storage::GetEnabledSensors(SensorTypes::Temperature))
        {
            sensors[std::to_string(Backend::SensorTypes::Temperature)] = (int)Climate::GetTemperature().GetCurrent();
        }

        if (Storage::GetEnabledSensors(SensorTypes::Humidity))
        {
            sensors[std::to_string(Backend::SensorTypes::Humidity)] = (int)Climate::GetHumidity().GetCurrent();
        }

        if (Storage::GetEnabledSensors(SensorTypes::AirPressure))
        {
            sensors[std::to_string(Backend::SensorTypes::AirPressure)] = (int)Climate::GetAirPressure().GetCurrent();
        }

        if (Storage::GetEnabledSensors(SensorTypes::GasResistance))
        {
            sensors[std::to_string(Backend::SensorTypes::GasResistance)] = (int)Climate::GetGasResistance().GetCurrent();
        }

        if (Storage::GetEnabledSensors(SensorTypes::Altitude))
        {
            sensors[std::to_string(Backend::SensorTypes::Altitude)] = (int)Climate::GetAltitude().GetCurrent();
        }

        if (Storage::GetEnabledSensors(SensorTypes::Sound))
        {
            sensors[std::to_string(Backend::SensorTypes::Sound)] = (int)Sound::GetMaxLevel();
        }

        // if (Storage::GetEnabledSensors(SensorTypes::RPM))
        // {
        //     sensors[std::to_string(Backend::SensorTypes::RPM)] = (int)RPM::GetRPM();
        // }

        std::string payload;
        serializeJson(doc, payload);

        Request request = Request(Storage::GetAddress() + Backend::ReadingURL, payload);
        if (request.POST())
        {
            return true;
        }

        Failsafe::AddFailure(TAG, "Registering readings failed");
    }

    return false;
}