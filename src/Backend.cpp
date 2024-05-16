#include "ArduinoJson.h"
#include "Definitions.h"
#include "Backend.h"
#include "Failsafe.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"
#include "Sound.h"

static const char *TAG = "Backend";

bool Backend::StatusOK(int statusCode)
{
    if (statusCode > 99 && statusCode < 400)
    {
        return true;
    }

    return false;
}

bool Backend::IsRedirect(int statusCode)
{
    if (statusCode == 300 || statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308)
    {
        return true;
    }

    return false;
}

bool Backend::CheckResponseFailed(std::string &payload, int statusCode)
{
    if (StatusOK(statusCode))
    {
        ESP_LOGI(TAG, "Backend response ok - status code: %d", statusCode);
        return false;
    }

    if (payload.empty())
    {
        Failsafe::AddFailure("Backend response failed - empty response - status code: " + std::to_string(statusCode));
        return true;
    }

    ESP_LOGI(TAG, "Checking backend response");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok)
    {
        Failsafe::AddFailure("Deserializing response failed: " + std::string(error.c_str()) + " - status code: " + std::to_string(statusCode));
        return true;
    }

    const char *errorMsg = doc["error"];

    if (errorMsg)
    {
        Failsafe::AddFailure("Backend response failed - status code: " + std::to_string(statusCode) + " - error: " + errorMsg);
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
        Failsafe::AddFailure("Deserializing setup configuration failed: " + std::string(error.c_str()));
        return false;
    }

    std::string address = doc["address"].as<const char *>();
    Helpers::RemoveWhiteSpace(address);
    if (address.back() != '/')
    {
        address += "/";
    }

    Storage::SetSSID(doc["ssid"].as<const char *>());
    Storage::SetPassword(doc["pass"].as<const char *>());
    Storage::SetAddress(address);
    Storage::SetDeviceId(doc["deviceid"]);
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
            Failsafe::AddFailure("Deserializing configuration failed: " + std::string(error.c_str()));
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
        sensors[std::to_string(Backend::SensorTypes::Sound)] = (int)Sound::GetMaxLevel();

        std::string payload;
        serializeJson(doc, payload);

        Request request = Request(Storage::GetAddress() + Backend::ReadingURL, payload);
        if (request.POST())
        {
            return true;
        }

        Failsafe::AddFailure("Registering readings failed");
    }

    return false;
}