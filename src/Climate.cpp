#include <cmath>
#include <string.h>

#include "bme680.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "Climate.h"
#include "Failsafe.h"
#include "Gui.h"

namespace Climate
{
    namespace Constants
    {
        static const float TemperatureOffset = 0,
                           HumidityOffset = 0,
                           AirPressureOffset = 0,
                           GasResistanceOffset = 0,
                           AltitudeOffset = 0,
                           SeaLevelPressure = 1026, SeaLevelTemperature = 9;
    };

    static const char *TAG = "Climate";
    static TaskHandle_t xHandle = nullptr;
    static bme680_sensor_t *dev = nullptr;
    static bme680_values_float_t values = {0};

    static Reading temperature, humidity, airPressure, gasResistance, altitude;
    static uint32_t duration = 0;
    static bool isOK = false;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

        dev = bme680_init_sensor(I2C_NUM_0, BME680_I2C_ADDRESS_2, 0);
        if (!dev)
        {
            Failsafe::AddFailure(TAG, "No sensor detected");
            vTaskDelete(nullptr);
        }

        bme680_set_oversampling_rates(dev, osr_16x, osr_16x, osr_16x);
        bme680_set_filter_size(dev, iir_size_127);
        bme680_set_heater_profile(dev, 0, 320, 25);
        bme680_use_heater_profile(dev, 0);

        duration = bme680_get_measurement_duration(dev);
        isOK = true;

        for (;;)
        {
            Update();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelete(nullptr);
    }

    static float calculateAltitude(float currentPressure, float seaLevelPressure, float seaLevelTemp)
    {
        const float L = 0.0065f, R = 8.31432f, g = 9.80665f;
        float temp = seaLevelTemp + 273.15f;
        float alt = (1.0f - powf(((currentPressure) / seaLevelPressure), (R * L) / (g * 0.0289644f))) * temp / L;
        return alt;
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 2, &xHandle);
    }

    void Update()
    {
        if (bme680_force_measurement(dev))
        {
            vTaskDelay(duration);
            if (bme680_get_results_float(dev, &values))
            {
                isOK = true;
                temperature.Update(values.temperature + Constants::TemperatureOffset);
                humidity.Update(values.humidity + Constants::HumidityOffset);

                if (values.pressure != 0)
                {
                    airPressure.Update(values.pressure + Constants::AirPressureOffset);
                    altitude.Update(calculateAltitude(airPressure.Current(), Constants::SeaLevelPressure, Constants::SeaLevelTemperature) + Constants::AltitudeOffset);
                }

                if (values.gas_resistance != 0)
                    gasResistance.Update(values.gas_resistance + Constants::GasResistanceOffset);

                // ESP_LOGI(TAG, "Temperature: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: %f, Altitude: %f", temperature.Current(), humidity.Current(), airPressure.Current(), gasResistance.Current(), altitude.Current());
            }

            else
            {
                isOK = false;
                Failsafe::AddFailureDelayed(TAG, "Getting result failed");
                vTaskDelete(nullptr);
            }
        }

        else
        {
            isOK = false;
            Failsafe::AddFailure(TAG, "Taking measurement failed");
            vTaskDelete(nullptr);
        }
    }

    void ResetValues(Configuration::Sensor::Sensors sensor)
    {
        using Sensors = Configuration::Sensor::Sensors;

        switch (sensor)
        {
        case Sensors::Temperature:
            temperature.Reset();
            break;
        case Sensors::Humidity:
            humidity.Reset();
            break;
        case Sensors::GasResistance:
            gasResistance.Reset();
            break;
        case Sensors::AirPressure:
            airPressure.Reset();
            break;
        case Sensors::Altitude:
            altitude.Reset();
            break;
        default:
            break;
        }
    }

    bool IsOK() { return isOK; }
    const Reading &GetTemperature() { return temperature; }
    const Reading &GetHumidity() { return humidity; }
    const Reading &GetAirPressure() { return airPressure; }
    const Reading &GetGasResistance() { return gasResistance; }
    const Reading &GetAltitude() { return altitude; }
}
