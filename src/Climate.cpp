#include <string.h>
#include <cmath>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "bme680.h"
#include "Climate.h"
#include "Failsafe.h"

static const char *TAG = "Climate";
static TaskHandle_t xHandle = nullptr;
static bme680_t *dev = nullptr;
static uint32_t duration = 0;

static bool init()
{
    esp_err_t err = i2cdev_init();
    if (err != ESP_OK)
    {
        Failsafe ::AddFailure(TAG, "Failed to initialize i2c driver");
        return false;
    }

    dev = new bme680_t;
    memset(dev, 0, sizeof(bme680_t));

    err = bme680_init_desc(dev, BME680_I2C_ADDR_1, I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22);
    if (err != ESP_OK)
    {
        Failsafe ::AddFailure(TAG, "Failed to initialize driver");
        return false;
    }

    err = bme680_init_sensor(dev);
    if (err != ESP_OK)
    {
        Failsafe ::AddFailure(TAG, "Failed to initialize BME680");
        return false;
    }

    bme680_settings_t sensor_settings = {
        .osr_temperature = BME680_OSR_8X,
        .osr_pressure = BME680_OSR_4X,
        .osr_humidity = BME680_OSR_2X,
        .filter_size = BME680_IIR_SIZE_127,
    };

    bme680_set_oversampling_rates(dev, sensor_settings.osr_temperature, sensor_settings.osr_pressure, sensor_settings.osr_humidity);
    bme680_set_filter_size(dev, sensor_settings.filter_size);
    bme680_set_heater_profile(dev, 0, 320, 150);
    bme680_use_heater_profile(dev, 0);
    bme680_get_measurement_duration(dev, &duration);

    return true;
}

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    if (init())
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

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
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY, &xHandle);
}

void Climate::Update()
{
    static bme680_values_float_t values = {0};

    if (bme680_force_measurement(dev) == ESP_OK)
    {
        vTaskDelay(duration);
        if (bme680_get_results_float(dev, &values) == ESP_OK)
        {
            m_Temperature.Update(values.temperature + TemperatureOffset);
            m_Humidity.Update(values.humidity + HumidityOffset);
            m_AirPressure.Update(values.pressure + AirPressureOffset);
            m_GasResistance.Update(values.gas_resistance + GasResistanceOffset);
            m_Altitude.Update(44330.0 * (1.0 - pow(values.pressure / SeaLevelPressure, 0.1903)) + AltitudeOffset);

            // ESP_LOGI(TAG, "Temp: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: %f, Altitude: %f", m_Temperature.GetCurrent(), m_Humidity.GetCurrent(), m_AirPressure.GetCurrent(), m_GasResistance.GetCurrent(), m_Altitude.GetCurrent());
        }
    }
}

void Climate::ResetValues(Backend::SensorTypes sensor)
{
    switch (sensor)
    {
    case Backend::SensorTypes::Temperature:
        m_Temperature.Reset();
        break;
    case Backend::SensorTypes::Humidity:
        m_Humidity.Reset();
        break;
    case Backend::SensorTypes::GasResistance:
        m_GasResistance.Reset();
        break;
    case Backend::SensorTypes::AirPressure:
        m_AirPressure.Reset();
        break;
    case Backend::SensorTypes::Altitude:
        m_Altitude.Reset();
        break;
    default:
        break;
    }
}