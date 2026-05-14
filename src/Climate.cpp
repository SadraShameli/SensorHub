#include "Climate.h"

#include <string.h>

#include <cmath>

#include "Configuration.h"
#include "Failsafe.h"
#include "Gui.h"
#include "Storage.h"
#include "bme680.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"

namespace Climate {
namespace Constants {

static const float TemperatureOffset = 0.0f, HumidityOffset = 0.0f,
                   AirPressureOffset = 0.0f, GasResistanceOffset = 0.0f,
                   AltitudeOffset = 0.0f, SeaLevelPressure = 1026.0f,
                   SeaLevelTemperature = 9.0f;
};

static const char* TAG = "Climate";
static TaskHandle_t xHandle = nullptr;

static bme680_sensor_t* dev = nullptr;
static bme680_values_float_t values = {0};

static Reading temperature, humidity, airPressure, gasResistance, altitude;
static uint32_t duration = 0;
static bool isOK = false;

namespace {

class ClimateSensor : public Sensors::ISensor {
   public:
    ClimateSensor(uint8_t id, const char* name, const char* unit,
                  Reading& reading)
        : m_id(id), m_name(name), m_unit(unit), m_reading(&reading) {}

    const char* Name() const override { return m_name; }

    const char* Unit() const override { return m_unit; }

    uint8_t Id() const override { return m_id; }

    bool IsOk() const override { return isOK; }

    Reading Snapshot() const override {
        return Reading(m_reading->Current(),
                       m_reading->Min(),
                       m_reading->Max());
    }

    void ResetMinMax() override { m_reading->Reset(); }

   private:
    uint8_t m_id;
    const char* m_name;
    const char* m_unit;
    Reading* m_reading;
};

ClimateSensor s_temperature{Configuration::Sensor::Temperature,
                            "Temperature",
                            "c",
                            temperature};
ClimateSensor s_humidity{Configuration::Sensor::Humidity,
                         "Humidity",
                         "%",
                         humidity};
ClimateSensor s_airPressure{Configuration::Sensor::AirPressure,
                            "Air Pressure",
                            " hPa",
                            airPressure};
ClimateSensor s_gasResistance{Configuration::Sensor::GasResistance,
                              "Gas Resistance",
                              " Ohms",
                              gasResistance};
ClimateSensor s_altitude{Configuration::Sensor::Altitude,
                         "Altitude",
                         "m",
                         altitude};

}

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    dev = bme680_init_sensor(I2C_NUM_0, BME680_I2C_ADDRESS_2, 0);
    if (dev == nullptr) {
        ESP_LOGW(TAG, "No sensor detected, skipping");
        vTaskDelete(nullptr);
    }

    bme680_set_oversampling_rates(dev, osr_16x, osr_16x, osr_16x);
    bme680_set_filter_size(dev, iir_size_127);
    bme680_set_heater_profile(dev, 0, 320, 25);
    bme680_use_heater_profile(dev, 0);

    duration = bme680_get_measurement_duration(dev);
    isOK = true;

    for (;;) {
        Update();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(nullptr);
}

static void RegisterSensors() {
    auto& registry = ::Sensors::SensorRegistry::Instance();
    registry.Register(&s_temperature);
    registry.Register(&s_humidity);
    registry.Register(&s_airPressure);
    registry.Register(&s_gasResistance);
    registry.Register(&s_altitude);
}

static bool ShouldStart() {
    using S = Configuration::Sensor::Sensors;
    return Storage::GetSensorState(S::Temperature) ||
           Storage::GetSensorState(S::Humidity) ||
           Storage::GetSensorState(S::AirPressure) ||
           Storage::GetSensorState(S::GasResistance) ||
           Storage::GetSensorState(S::Altitude);
}

void Init() {
    RegisterSensors();
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 2, &xHandle);
}

const Kernel::Service kService = {
    .name = "Climate",
    .modes = Kernel::RunInNormalMode,
    .on_init = &RegisterSensors,
    .task_entry = &vTask,
    .stack_bytes = 4096,
    .priority = tskIDLE_PRIORITY + 2,
    .out_handle = &xHandle,
    .should_start = &ShouldStart,
};

void Update() {
    if (!bme680_force_measurement(dev)) {
        Failsafe::AddFailure(TAG, "Taking measurement failed");

        isOK = false;
        vTaskDelay(pdMS_TO_TICKS(1000));

        return;
    }

    vTaskDelay(pdMS_TO_TICKS(duration ? duration : 50));

    if (!bme680_get_results_float(dev, &values)) {
        Failsafe::AddFailureDelayed(TAG, "Getting result failed");

        isOK = false;
        vTaskDelay(pdMS_TO_TICKS(1000));

        return;
    }

    temperature.Update(values.temperature + Constants::TemperatureOffset);
    humidity.Update(values.humidity + Constants::HumidityOffset);

    if (values.pressure != 0) {

        const float freshPressure =
            values.pressure + Constants::AirPressureOffset;
        const float alt = calculateAltitude(freshPressure,
                                            Constants::SeaLevelPressure,
                                            Constants::SeaLevelTemperature) +
                          Constants::AltitudeOffset;

        airPressure.Update(freshPressure);
        altitude.Update(alt);
    }

    if (values.gas_resistance != 0) {
        gasResistance.Update(values.gas_resistance +
                             Constants::GasResistanceOffset);
    }

    isOK = true;

    ESP_LOGD(TAG,
             "Temperature: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: "
             "%f, Altitude: %f",
             temperature.Current(),
             humidity.Current(),
             airPressure.Current(),
             gasResistance.Current(),
             altitude.Current());
}

void ResetValues(Configuration::Sensor::Sensors sensor) {
    using Sensors = Configuration::Sensor::Sensors;

    switch (sensor) {
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

float calculateAltitude(float currentPressure, float seaLevelPressure,
                        float seaLevelTemp) {

    const float L = 0.0065f;

    const float R = 8.31432f;

    const float G = 9.80665f;

    float seaLevelTempK = seaLevelTemp + 273.15f;

    float alt = (1.0f - powf(((currentPressure) / seaLevelPressure),
                             (R * L) / (G * 0.0289644f))) *
                seaLevelTempK / L;

    return alt;
}

bool IsOK() {
    return isOK;
}

const Reading& GetTemperature() {
    return temperature;
}

const Reading& GetHumidity() {
    return humidity;
}

const Reading& GetAirPressure() {
    return airPressure;
}

const Reading& GetGasResistance() {
    return gasResistance;
}

const Reading& GetAltitude() {
    return altitude;
}

}
