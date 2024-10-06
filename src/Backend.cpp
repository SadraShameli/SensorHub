#include "ArduinoJson.h"
#include "freertos/FreeRTOS.h"

#include "Backend.h"
#include "Climate.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "HTTP.h"
#include "Mic.h"
#include "Network.h"
#include "Storage.h"
#include "WiFi.h"

namespace Backend
{
    using Sensors = Configuration::Sensor::Sensors;

    static const char *TAG = "Backend";

    std::string DeviceURL = "device/", ReadingURL = "reading/", RecordingURL = "recording/";

    bool CheckResponseFailed(const std::string &payload, int statusCode)
    {
        ESP_LOGI(TAG, "Checking response");

        if (HTTP::StatusOK(statusCode))
        {
            ESP_LOGI(TAG, "Response ok");
            return false;
        }

        if (payload.empty())
        {
            Failsafe::AddFailure(TAG, "Status: " + std::to_string(statusCode) + " - empty response");
            return true;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error != DeserializationError::Ok)
        {
            Failsafe::AddFailure(TAG, "Status: " + std::to_string(statusCode) + " - deserialization failed: " + std::string(error.c_str()));
            return true;
        }

        const char *errorMsg = doc["error"];
        if (errorMsg)
            Failsafe::AddFailure(TAG, "Status: " + std::to_string(statusCode) + " - " + errorMsg);

        return true;
    }

    bool SetupConfiguration(const std::string &payload)
    {
        ESP_LOGI(TAG, "Setting up configuration: %s", payload.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error != DeserializationError::Ok)
        {
            Failsafe::AddFailure(TAG, "Deserialization failed: " + std::string(error.c_str()));
            return false;
        }

        std::string ssid = doc["ssid"];
        if (ssid.length() > 32 || ssid.empty())
        {
            Failsafe::AddFailure(TAG, "Invalid WiFi SSID");
            return false;
        }
        Storage::SetSSID(std::move(ssid));

        std::string password = doc["pass"];
        if (!password.empty() && (password.length() < 8 || password.length() > 64))
        {
            Failsafe::AddFailure(TAG, "Invalid WiFi Password");
            return false;
        }
        Storage::SetPassword(std::move(password));

        uint32_t device_id = doc["device_id"];
        if (!device_id)
        {
            Failsafe::AddFailure(TAG, "Device Id can't be empty");
            return false;
        }
        Storage::SetDeviceId(device_id);

        std::string address = doc["address"];
        if (address.empty())
        {
            Failsafe::AddFailure(TAG, "Address can't be empty");
            return false;
        }
        Helpers::RemoveWhiteSpace(address);
        if (address.back() != '/')
            address += "/";
        Storage::SetAddress(std::move(address));

        Network::NotifyConfigSet();
        return true;
    }

    void GetConfiguration()
    {
        ESP_LOGI(TAG, "Fetching configuration");

        HTTP::Request request(Storage::GetAddress() + DeviceURL + std::to_string(Storage::GetDeviceId()));
        if (request.GET())
        {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, request.GetResponse());

            if (error != DeserializationError::Ok)
            {
                Failsafe::AddFailure(TAG, "Deserialization failed: " + std::string(error.c_str()));
                return;
            }

            Storage::SetDeviceName(doc["name"]);
            Storage::SetDeviceId(doc["device_id"]);
            Storage::SetRegisterInterval(doc["register_interval"]);
            Storage::SetLoudnessThreshold(doc["loudness_threshold"]);

            JsonArray sensors = doc["sensors"].as<JsonArray>();
            for (JsonVariant sensor : sensors)
                Storage::SetSensorState(sensor.as<Sensors>(), true);

            Storage::SetConfigMode(false);
            Storage::Commit();
            esp_restart();
        }

        ESP_LOGE(TAG, "Fetching configuration failed");
    }

    bool RegisterReadings()
    {
        if (WiFi::IsConnected())
        {
            ESP_LOGI(TAG, "Registering readings");

            JsonDocument doc;
            JsonObject sensors = doc["sensors"].to<JsonObject>();
            doc["device_id"] = Storage::GetDeviceId();

            if (Climate::IsOK())
            {
                if (Storage::GetSensorState(Sensors::Temperature))
                    sensors[std::to_string(Sensors::Temperature)] = (int)Climate::GetTemperature().Current();

                if (Storage::GetSensorState(Sensors::Humidity))
                    sensors[std::to_string(Sensors::Humidity)] = (int)Climate::GetHumidity().Current();

                if (Storage::GetSensorState(Sensors::AirPressure))
                    sensors[std::to_string(Sensors::AirPressure)] = (int)Climate::GetAirPressure().Current();

                if (Storage::GetSensorState(Sensors::GasResistance))
                    sensors[std::to_string(Sensors::GasResistance)] = (int)Climate::GetGasResistance().Current();

                if (Storage::GetSensorState(Sensors::Altitude))
                    sensors[std::to_string(Sensors::Altitude)] = (int)Climate::GetAltitude().Current();
            }

            if (Mic::IsOK())
            {
                if (Storage::GetSensorState(Sensors::Loudness))
                    sensors[std::to_string(Sensors::Loudness)] = (int)Mic::GetLoudness().Max();
            }

            // if (Storage::GetSensorState(Sensors::RPM))
            //     sensors[std::to_string(Sensors::RPM)] = (int)RPM::GetRPM();

            if (!sensors.size())
            {
                Failsafe::AddFailure(TAG, "No sensor values to register");
                vTaskDelete(nullptr);
            }

            std::string payload;
            serializeJson(doc, payload);

            HTTP::Request request(Storage::GetAddress() + ReadingURL);
            if (request.POST(payload))
                return true;

            Failsafe::AddFailure(TAG, "Registering readings failed");
        }

        return false;
    }
}
