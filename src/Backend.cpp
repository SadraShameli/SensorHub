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

bool Backend::CheckResponseFailed(std::string &payload, int statusCode)
{
    if (StatusOK(statusCode))
    {
        ESP_LOGI(TAG, "Backend response ok - status code: %d", statusCode);
        return false;
    }

    if (payload.empty())
    {
        Failsafe::AddFailure({.Message = "Backend response failed - status code: " + std::to_string(statusCode)});
        return true;
    }

    ESP_LOGI(TAG, "Checking backend response");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok)
    {
        Failsafe::AddFailure({.Message = "Deserializing response failed", .Error = error.c_str()});
        return true;
    }

    const char *errorMsg = doc["error"];

    if (errorMsg)
    {
        Failsafe::AddFailure({.Message = "Backend response failed - status code: " + std::to_string(statusCode), .Error = errorMsg});
    }

    return true;
}

bool Backend::SetupConfiguration(std::string &payload)
{
    ESP_LOGI(TAG, "Setting up configuration - Payload: %s", payload.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok)
    {
        Failsafe::AddFailure({.Message = "Deserializing setup configuration failed", .Error = error.c_str()});
        return false;
    }

    Backend::SSID = doc["ssid"].as<const char *>();
    Backend::Password = doc["pass"].as<const char *>();
    Backend::DeviceId = doc["deviceid"].as<const char *>();
    Backend::Address = doc["address"].as<const char *>();

    Helpers::RemoveWhiteSpace(Backend::DeviceId);
    Helpers::RemoveWhiteSpace(Backend::Address);

    ESP_LOGI(TAG, "Read Device Id from webpage: %s", Backend::DeviceId.c_str());
    ESP_LOGI(TAG, "Read Address from webpage: %s", Backend::Address.c_str());

    Network::NotifyConfigSet();

    return true;
}

bool Backend::GetConfiguration()
{
    ESP_LOGI(TAG, "Getting configuration");

    std::string payload, url = Address + DeviceURL + DeviceId;

    if (HTTP::GET(url.c_str(), payload))
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error != DeserializationError::Ok)
        {
            Failsafe::AddFailure({.Message = "Deserializing configuration failed", .Error = error.c_str()});
            return false;
        }

        DeviceId = std::to_string(doc["device_id"].as<int>());
        DeviceName = doc["name"].as<const char *>();
        RegisterInterval = doc["register_interval"].as<int>();
        LoudnessThreshold = doc["loudness_threshold"].as<int>();

        return true;
    }

    ESP_LOGE(TAG, "Getting configuration failed");

    return false;
}

bool Backend::RegisterReadings()
{
    if (WiFi::IsConnected())
    {
        ESP_LOGI(TAG, "Registering readings");

        JsonDocument doc;
        doc["device_id"] = Backend::DeviceId;

        JsonObject sensors = doc["sensors"].to<JsonObject>();
        sensors[Backend::SensorTypes::Sound == 6 ? "6" : 0] = (int)Sound::GetMaxLevel();

        std::string payload, url = Address + Backend::ReadingURL;
        serializeJson(doc, payload);

        if (HTTP::POST(url.c_str(), payload))
        {
            return true;
        }

        Failsafe::AddFailure({.Message = "Register readings failed"});
    }

    return false;
}