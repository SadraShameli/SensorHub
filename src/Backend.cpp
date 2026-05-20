#include "Backend.h"

#include <cstdint>
#include <string>

#include "ArduinoJson.h"
#include "Climate.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "HTTP.h"
#include "Mic.h"
#include "Network.h"
#include "Storage.h"
#include "WiFi.h"
#include "freertos/FreeRTOS.h"
#include "sensorhub_core/UrlValidator.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"

namespace Backend {

static const char* TAG = "Backend";

std::string DeviceURL = "device/", ReadingURL = "reading/",
            RecordingURL = "recording/";

bool CheckResponseFailed(const std::string& payload,
                         HTTP::Status::StatusCode statusCode) {
    if (HTTP::Status::IsSuccess(statusCode)) {
        ESP_LOGI(TAG, "Response ok");
        return false;
    }

    if (payload.empty()) {
        Failsafe::AddFailure(
            TAG,
            "Status: " + std::to_string((int)statusCode) + " - empty response");
        return true;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error != DeserializationError::Ok) {
        Failsafe::AddFailure(TAG,
                             "Status: " + std::to_string((int)statusCode) +
                                 " - deserialization failed");
        return true;
    }

    const char* err = doc["error"];
    if (err != nullptr) {
        Failsafe::AddFailure(
            TAG,
            "Status: " + std::to_string((int)statusCode) + " - " + err);
    }

    return true;
}

bool SetupConfiguration(const std::string& payload) {
    ESP_LOGI(TAG,
             "Setting up configuration (%u bytes)",
             (unsigned)payload.size());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error != DeserializationError::Ok) {
        Failsafe::AddFailure(TAG, "Deserialization failed");
        return false;
    }

    std::string ssid = doc["ssid"].as<std::string>();
    if (ssid.length() > 32 || ssid.empty()) {
        Failsafe::AddFailure(TAG, "Invalid WiFi SSID");
        return false;
    }
    Storage::SetSSID(std::move(ssid));

    std::string password = doc["pass"].as<std::string>();
    if (!password.empty() &&
        (password.length() < 8 || password.length() > 64)) {
        Failsafe::AddFailure(TAG, "Invalid WiFi Password");
        return false;
    }
    Storage::SetPassword(std::move(password));

    const uint32_t device_id = doc["device_id"].as<uint32_t>();
    if (device_id == 0) {
        Failsafe::AddFailure(TAG, "Device Id can't be empty");
        return false;
    }
    Storage::SetDeviceId(device_id);

    std::string address = doc["address"].as<std::string>();
    if (address.empty()) {
        Failsafe::AddFailure(TAG, "Address can't be empty");
        return false;
    }
    Helpers::RemoveWhiteSpace(address);

    sensorhub::core::UrlValidatorOptions urlOpts;
#ifdef UNIT_DEBUG
    urlOpts.allow_ip_literal = true;
    urlOpts.require_dot_in_host = false;
#endif
    if (!sensorhub::core::IsAllowedBackendUrl(address, urlOpts)) {
        Failsafe::AddFailure(TAG, "Backend URL rejected; must be https://");
        return false;
    }

    if (address.back() != '/') {
        address += "/";
    }
    Storage::SetAddress(std::move(address));

    std::string token = doc["token"].as<std::string>();
    if (token.empty() || token.length() >= 64) {
        Failsafe::AddFailure(TAG, "Invalid device token");
        return false;
    }
    Storage::SetAuthKey(std::move(token));

    Network::NotifyConfigSet();
    return true;
}

ProbeResult ProbeAndStoreConfiguration() {
    ESP_LOGI(TAG, "Fetching configuration");

    HTTP::Request request(Storage::GetAddress() + DeviceURL +
                          std::to_string(Storage::GetDeviceId()));

    if (!request.GET()) {
        return {false, "Backend unreachable at " + Storage::GetAddress()};
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request.GetResponse());
    if (error != DeserializationError::Ok) {
        return {false, "Backend returned invalid JSON"};
    }

    Storage::SetDeviceName(doc["name"].as<std::string>());
    Storage::SetDeviceId(doc["device_id"].as<uint32_t>());
    Storage::SetRegisterInterval(doc["register_interval"].as<uint32_t>());
    Storage::SetLoudnessThreshold(doc["loudness_threshold"].as<uint32_t>());

    JsonArray sensors = doc["sensors"].as<JsonArray>();
    for (JsonVariant sensor : sensors) {
        Storage::SetSensorState(
            static_cast<Configuration::Sensor::Sensors>(sensor.as<int>()),
            true);
    }

    Storage::SetConfigMode(false);
    Storage::Commit();
    return {true, ""};
}

bool RegisterReadings() {
    if (!WiFi::IsConnected()) {
        return false;
    }

    ESP_LOGI(TAG, "Registering readings");

    JsonDocument doc;
    JsonObject sensorsObj = doc["sensors"].to<JsonObject>();
    doc["device_id"] = Storage::GetDeviceId();

    int sensorCount = 0;
    const auto& registry = ::Sensors::SensorRegistry::Instance();
    for (auto it = registry.Begin(); it != registry.End(); ++it) {
        const auto* sensor = *it;
        const auto cfgId =
            static_cast<Configuration::Sensor::Sensors>(sensor->Id());

        if (!Storage::GetSensorState(cfgId) || !sensor->IsOk()) {
            continue;
        }

        sensorsObj[std::to_string(sensor->Id())] = sensor->ReportingValue();
        ++sensorCount;
    }

    if (sensorCount == 0) {
        Failsafe::AddFailure(TAG, "No sensor values to register");
        vTaskDelete(nullptr);
    }

    std::string payload;
    serializeJson(doc, payload);

    HTTP::Request request(Storage::GetAddress() + ReadingURL);
    if (request.POST(payload, Storage::GetAuthKey())) {
        return true;
    }

    Failsafe::AddFailure(TAG, "Registering readings failed");
    return false;
}

}
