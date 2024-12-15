#include "Backend.h"

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

namespace Backend {

static const char *TAG = "Backend";

std::string DeviceURL = "device/", ReadingURL = "reading/",
            RecordingURL = "recording/";

/**
 * @brief Checks if the response from an HTTP request indicates a failure.
 *
 * This function logs the status of the response and checks if the status code
 * indicates success. If the status code is not OK, it further checks if the
 * payload is empty or if there was an error deserializing the JSON payload.
 * If any of these conditions are met, it logs the failure and returns true.
 *
 * @param payload The response payload as a string.
 * @param statusCode The HTTP status code of the response.
 * @return true if the response indicates a failure, false otherwise.
 */
bool CheckResponseFailed(
    const std::string &payload, HTTP::Status::StatusCode statusCode
) {
    ESP_LOGI(TAG, "Checking response");

    if (HTTP::Status::IsSuccess(statusCode)) {
        ESP_LOGI(TAG, "Response ok");

        return false;
    }

    if (payload.empty()) {
        Failsafe::AddFailure(
            TAG,
            "Status: " + std::to_string((int)statusCode) + " - empty response"
        );

        return true;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok) {
        Failsafe::AddFailure(
            TAG, "Status: " + std::to_string((int)statusCode) +
                     " - deserialization failed: " + std::string(error.c_str())
        );

        return true;
    }

    const char *errorMsg = doc["error"];
    if (errorMsg != nullptr) {
        Failsafe::AddFailure(
            TAG, "Status: " + std::to_string((int)statusCode) + " - " + errorMsg
        );
    }

    return true;
}

/**
 * @brief Sets up the configuration for the device using the provided JSON
 * payload.
 *
 * This function deserializes the JSON payload and extracts the necessary
 * configuration parameters such as WiFi SSID, password, device ID, and address.
 * It validates these parameters and stores them in the appropriate storage. If
 * any parameter is invalid, it logs the failure and returns false.
 *
 * @param payload A JSON string containing the configuration parameters.
 * @return true if the configuration is successfully set up, false otherwise.
 */
bool SetupConfiguration(const std::string &payload) {
    ESP_LOGI(TAG, "Setting up configuration: %s", payload.c_str());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error != DeserializationError::Ok) {
        Failsafe::AddFailure(
            TAG, "Deserialization failed: " + std::string(error.c_str())
        );

        return false;
    }

    std::string ssid = doc["ssid"];
    if (ssid.length() > 32 || ssid.empty()) {
        Failsafe::AddFailure(TAG, "Invalid WiFi SSID");

        return false;
    }
    Storage::SetSSID(std::move(ssid));

    std::string password = doc["pass"];
    if (!password.empty() &&
        (password.length() < 8 || password.length() > 64)) {
        Failsafe::AddFailure(TAG, "Invalid WiFi Password");

        return false;
    }
    Storage::SetPassword(std::move(password));

    uint32_t device_id = doc["device_id"];
    if (device_id == 0) {
        Failsafe::AddFailure(TAG, "Device Id can't be empty");

        return false;
    }
    Storage::SetDeviceId(device_id);

    std::string address = doc["address"];
    if (address.empty()) {
        Failsafe::AddFailure(TAG, "Address can't be empty");

        return false;
    }
    Helpers::RemoveWhiteSpace(address);
    if (address.back() != '/') {
        address += "/";
    }
    Storage::SetAddress(std::move(address));

    Network::NotifyConfigSet();

    return true;
}

/**
 * @brief Fetches the device configuration from a remote server and updates the
 * local storage.
 *
 * This function sends an HTTP GET request to retrieve the device configuration.
 * If the request is successful, it deserializes the JSON response and updates
 * the local storage with the new configuration values. If deserialization
 * fails, it logs the failure and exits the function. If the configuration is
 * successfully updated, the device is restarted.
 *
 * The configuration includes:
 *
 * - Device name
 *
 * - Device ID
 *
 * - Register interval
 *
 * - Loudness threshold
 *
 * - Sensor states
 *
 * @note This function will restart the device if the configuration is
 * successfully updated.
 */
void GetConfiguration() {
    ESP_LOGI(TAG, "Fetching configuration");

    HTTP::Request request(
        Storage::GetAddress() + DeviceURL +
        std::to_string(Storage::GetDeviceId())
    );

    if (request.GET()) {
        JsonDocument doc;
        DeserializationError error =
            deserializeJson(doc, request.GetResponse());

        if (error != DeserializationError::Ok) {
            Failsafe::AddFailure(
                TAG, "Deserialization failed: " + std::string(error.c_str())
            );

            return;
        }

        Storage::SetDeviceName(doc["name"]);
        Storage::SetDeviceId(doc["device_id"]);
        Storage::SetRegisterInterval(doc["register_interval"]);
        Storage::SetLoudnessThreshold(doc["loudness_threshold"]);

        JsonArray sensors = doc["sensors"].as<JsonArray>();

        for (JsonVariant sensor : sensors) {
            Storage::SetSensorState(
                sensor.as<Configuration::Sensor::Sensors>(), true
            );
        }

        Storage::SetConfigMode(false);
        Storage::Commit();

        esp_restart();
    }

    ESP_LOGE(TAG, "Fetching configuration failed");
}

/**
 * @brief Registers sensor readings to a remote server.
 *
 * This function collects sensor data from various sensors (temperature,
 * humidity, air pressure, gas resistance, altitude, and loudness) and sends the
 * data to a remote server if the device is connected to WiFi.
 *
 * @return `true` if the readings were successfully registered, `false`
 * otherwise.
 */
bool RegisterReadings() {
    if (WiFi::IsConnected()) {
        ESP_LOGI(TAG, "Registering readings");

        JsonDocument doc;
        JsonObject sensors = doc["sensors"].to<JsonObject>();

        doc["device_id"] = Storage::GetDeviceId();

        using Sensors = Configuration::Sensor::Sensors;

        if (Climate::IsOK()) {
            if (Storage::GetSensorState(Sensors::Temperature)) {
                sensors[std::to_string(Sensors::Temperature)] =
                    (int)Climate::GetTemperature().Current();
            }

            if (Storage::GetSensorState(Sensors::Humidity)) {
                sensors[std::to_string(Sensors::Humidity)] =
                    (int)Climate::GetHumidity().Current();
            }

            if (Storage::GetSensorState(Sensors::AirPressure)) {
                sensors[std::to_string(Sensors::AirPressure)] =
                    (int)Climate::GetAirPressure().Current();
            }

            if (Storage::GetSensorState(Sensors::GasResistance)) {
                sensors[std::to_string(Sensors::GasResistance)] =
                    (int)Climate::GetGasResistance().Current();
            }

            if (Storage::GetSensorState(Sensors::Altitude)) {
                sensors[std::to_string(Sensors::Altitude)] =
                    (int)Climate::GetAltitude().Current();
            }
        }

        if (Mic::IsOK()) {
            if (Storage::GetSensorState(Sensors::Loudness)) {
                sensors[std::to_string(Sensors::Loudness)] =
                    (int)Mic::GetLoudness().Max();
            }
        }

        if (sensors.size() == 0) {
            Failsafe::AddFailure(TAG, "No sensor values to register");

            vTaskDelete(nullptr);
        }

        std::string payload;
        serializeJson(doc, payload);

        HTTP::Request request(Storage::GetAddress() + ReadingURL);
        if (request.POST(payload)) {
            return true;
        }

        Failsafe::AddFailure(TAG, "Registering readings failed");
    }

    return false;
}

}  // namespace Backend
