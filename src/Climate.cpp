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

static bool init()
{
    dev = bme680_init_sensor(I2C_NUM_0, BME680_I2C_ADDRESS_2, 0);

    if (!dev)
    {
        Failsafe ::AddFailure(TAG, "Failed to initialize sensor");
        return false;
    }

    bme680_set_oversampling_rates(dev, osr_8x, osr_4x, osr_2x);
    bme680_set_filter_size(dev, iir_size_7);
    bme680_set_heater_profile(dev, 0, 320, 150);
    bme680_use_heater_profile(dev, 0);
    duration = bme680_get_measurement_duration(dev);

    return true;
}

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    if (init())
    {
        isOK = true;

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
            m_Temperature.Update(values.temperature + TemperatureOffset);
            m_Humidity.Update(values.humidity + HumidityOffset);
            m_AirPressure.Update(values.pressure + AirPressureOffset);
            m_GasResistance.Update(values.gas_resistance + GasResistanceOffset);
            m_Altitude.Update(44330.0 * (1.0 - pow(values.pressure / SeaLevelPressure, 0.1903)) + AltitudeOffset);

            // ESP_LOGI(TAG, "Temp: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: %f, Altitude: %f", m_Temperature.GetCurrent(), m_Humidity.GetCurrent(), m_AirPressure.GetCurrent(), m_GasResistance.GetCurrent(), m_Altitude.GetCurrent());
        }

        else
        {
            isOK = false;
            Failsafe::AddFailure(TAG, "Getting result failed");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    else
    {
        isOK = false;
        Failsafe::AddFailure(TAG, "Starting measurement failed");
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
