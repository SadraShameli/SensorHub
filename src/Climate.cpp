#include <string.h>
#include <cmath>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "bme680.h"
#include "Climate.h"
#include "Failsafe.h"
#include "Gui.h"

static const char *TAG = "Climate";
static TaskHandle_t xHandle = nullptr;

static bme680_sensor_t *dev = nullptr;
static uint32_t duration = 0;
static bool isOK = false;

static float calculateAltitude(float currentPressure, float seaLevelPressure, float seaLevelTemp)
{
    const float L = 0.0065, R = 8.31432, g = 9.80665;
    float temp = seaLevelTemp + 273.15;
    float alt = (1 - pow(((currentPressure) / seaLevelPressure), (R * L) / (g * 0.0289644))) * temp / L;
    return alt;
}

static bool init()
{
    dev = bme680_init_sensor(I2C_NUM_0, BME680_I2C_ADDRESS_2, 0);

    if (!dev)
    {
        Failsafe ::AddFailure(TAG, "Failed to initialize sensor");
        return false;
    }

    bme680_set_oversampling_rates(dev, osr_16x, osr_16x, osr_16x);
    bme680_set_filter_size(dev, iir_size_127);
    bme680_set_heater_profile(dev, 0, 320, 25);
    bme680_use_heater_profile(dev, 0);
    duration = bme680_get_measurement_duration(dev);

    isOK = true;

    return isOK;
}

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    if (init())
    {
        for (;;)
        {
            Climate::Update();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    vTaskDelete(nullptr);
}

void Climate::Init()
{
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 2, &xHandle);
}

void Climate::Update()
{
    static bme680_values_float_t values = {0};

    if (bme680_force_measurement(dev))
    {
        vTaskDelay(duration);
        if (bme680_get_results_float(dev, &values))
        {
            m_Temperature.Update(values.temperature + Climate::Constants::TemperatureOffset);
            m_Humidity.Update(values.humidity + Climate::Constants::HumidityOffset);

            if (values.pressure != 0)
            {
                m_AirPressure.Update(values.pressure + Climate::Constants::AirPressureOffset);
                m_Altitude.Update(calculateAltitude(m_AirPressure.Current(), Climate::Constants::SeaLevelPressure, Climate::Constants::SeaLevelTemperature) + Climate::Constants::AltitudeOffset);
            }

            if (values.gas_resistance != 0)
                m_GasResistance.Update(values.gas_resistance + Climate::Constants::GasResistanceOffset);

            // ESP_LOGI(TAG, "Temperature: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: %f, Altitude: %f", m_Temperature.Current(), m_Humidity.Current(), m_AirPressure.Current(), m_GasResistance.Current(), m_Altitude.Current());
        }

        else
        {
            isOK = false;
            Failsafe::AddFailureDelayed(TAG, "Getting result failed");
        }
    }

    else
    {
        isOK = false;
        Failsafe::AddFailure(TAG, "Taking measurement failed");
        vTaskDelete(nullptr);
    }
}

bool Climate::IsOK()
{
    return isOK;
}

void Climate::ResetValues(Configuration::Sensors::Sensor sensor)
{
    switch (sensor)
    {
    case Configuration::Sensors::Temperature:
        m_Temperature.Reset();
        break;
    case Configuration::Sensors::Humidity:
        m_Humidity.Reset();
        break;
    case Configuration::Sensors::GasResistance:
        m_GasResistance.Reset();
        break;
    case Configuration::Sensors::AirPressure:
        m_AirPressure.Reset();
        break;
    case Configuration::Sensors::Altitude:
        m_Altitude.Reset();
        break;
    default:
        break;
    }
}
