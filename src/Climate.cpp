#include "Climate.h"

#include <string.h>

#include <cmath>

#include "Failsafe.h"
#include "Gui.h"
#include "bme680.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

namespace Climate {
namespace Constants {

static const float TemperatureOffset = 0.0f, HumidityOffset = 0.0f,
                   AirPressureOffset = 0.0f, GasResistanceOffset = 0.0f,
                   AltitudeOffset = 0.0f, SeaLevelPressure = 1026.0f,
                   SeaLevelTemperature = 9.0f;
};

static const char *TAG = "Climate";
static TaskHandle_t xHandle = nullptr;

static bme680_sensor_t *dev = nullptr;
static bme680_values_float_t values = {0};

static Reading temperature, humidity, airPressure, gasResistance, altitude;
static uint32_t duration = 0;
static bool isOK = false;

/**
 * @brief Task function to initialize and periodically update the BME680 sensor.
 *
 * This function initializes the BME680 sensor with specified I2C parameters and
 * oversampling rates. It sets the filter size and heater profile for the
 * sensor. If the sensor initialization fails, it logs an error and deletes the
 * task. Once initialized, it enters an infinite loop where it updates the
 * sensor data and delays for a specified duration.
 *
 * @param arg Pointer to the task argument (unused).
 */
static void vTask(void *arg) {
    ESP_LOGI(TAG, "Initializing");

    dev = bme680_init_sensor(I2C_NUM_0, BME680_I2C_ADDRESS_2, 0);

    if (dev == nullptr) {
        Failsafe::AddFailure(TAG, "No sensor detected");

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

/**
 * @brief Initializes the climate control task.
 *
 * This function creates a new FreeRTOS task for climate control using the
 * specified task function, task name, stack size, priority, and task handle.
 *
 * @note The task is created with a priority of (tskIDLE_PRIORITY + 2) and a
 * stack size of 4096 bytes.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 4096, nullptr, tskIDLE_PRIORITY + 2, &xHandle);
}

/**
 * @brief Updates the sensor readings and processes the results.
 *
 * This function forces a measurement from the BME680 sensor and waits for the
 * specified duration. If the measurement is successful, it retrieves the
 * results and updates the temperature, humidity, air pressure, altitude, and
 * gas resistance values with the appropriate offsets. If the measurement or
 * result retrieval fails, it logs the failure and deletes the current task.
 *
 * @note The function uses FreeRTOS vTaskDelay and vTaskDelete for task
 * management.
 */
void Update() {
    if (!bme680_force_measurement(dev)) {
        Failsafe::AddFailure(TAG, "Taking measurement failed");

        isOK = false;
        vTaskDelay(pdMS_TO_TICKS(1000));

        return;
    }

    vTaskDelay(duration);

    if (!bme680_get_results_float(dev, &values)) {
        Failsafe::AddFailureDelayed(TAG, "Getting result failed");

        isOK = false;
        vTaskDelay(pdMS_TO_TICKS(1000));

        return;
    }

    temperature.Update(values.temperature + Constants::TemperatureOffset);
    humidity.Update(values.humidity + Constants::HumidityOffset);

    if (values.pressure != 0) {
        float alt = calculateAltitude(
                        airPressure.Current(), Constants::SeaLevelPressure,
                        Constants::SeaLevelTemperature
                    ) +
                    Constants::AltitudeOffset;

        altitude.Update(alt);
        airPressure.Update(values.pressure + Constants::AirPressureOffset);
    }

    if (values.gas_resistance != 0) {
        gasResistance.Update(
            values.gas_resistance + Constants::GasResistanceOffset
        );
    }

    isOK = true;

    ESP_LOGD(
        TAG,
        "Temperature: %f, Humidity: %f, Air Pressure: %f, Gas Resistance: "
        "%f, Altitude: %f",
        temperature.Current(), humidity.Current(), airPressure.Current(),
        gasResistance.Current(), altitude.Current()
    );
}

/**
 * @brief Resets the values of the specified sensor.
 *
 * This function resets the values of the sensor specified by the
 * parameter `sensor`. It supports the following sensor types:
 * - Temperature
 * - Humidity
 * - GasResistance
 * - AirPressure
 * - Altitude
 *
 * @param sensor The sensor whose values need to be reset. It should be
 *               one of the enumerators of `Configuration::Sensor::Sensors`.
 */
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

/**
 * @brief Calculates the altitude based on the current pressure, sea level
 * pressure, and sea level temperature.
 *
 * This function uses the barometric formula to estimate the altitude above sea
 * level.
 *
 * @param currentPressure The current atmospheric pressure in Pascals.
 * @param seaLevelPressure The standard atmospheric pressure at sea level in
 * Pascals.
 * @param seaLevelTemp The standard temperature at sea level in degrees Celsius.
 * @return The calculated altitude in meters.
 */
static float calculateAltitude(
    float currentPressure, float seaLevelPressure, float seaLevelTemp
) {
    // Lapse rate in K/m (temperature decrease per meter of altitude)
    const float L = 0.0065f;

    // Universal gas constant in J/(mol·K)
    const float R = 8.31432f;

    // Gravitational acceleration in m/s²
    const float G = 9.80665f;

    // Convert sea level temperature to Kelvin
    float seaLevelTempK = seaLevelTemp + 273.15f;

    float alt = (1.0f - powf(
                            ((currentPressure) / seaLevelPressure),
                            (R * L) / (G * 0.0289644f)
                        )) *
                seaLevelTempK / L;

    return alt;
}

/**
 * @brief Checks if the system status is OK.
 *
 * This function returns the status of the system, indicating whether it is
 * operating correctly.
 *
 * @return true if the system is OK, false otherwise.
 */
bool IsOK() { return isOK; }

/**
 * @brief Retrieves the current temperature reading.
 *
 * @return const Reading& Reference to the current temperature reading.
 */
const Reading &GetTemperature() { return temperature; }

/**
 * @brief Retrieves the current humidity reading.
 *
 * @return const Reading& Reference to the current humidity reading.
 */
const Reading &GetHumidity() { return humidity; }

/**
 * @brief Retrieves the current air pressure reading.
 *
 * @return const Reading& Reference to the air pressure reading.
 */
const Reading &GetAirPressure() { return airPressure; }

/**
 * @brief Retrieves the gas resistance reading.
 *
 * @return const Reading& Reference to the gas resistance reading.
 */
const Reading &GetGasResistance() { return gasResistance; }

/**
 * @brief Retrieves the altitude reading.
 *
 * This function returns a constant reference to the altitude reading.
 *
 * @return const Reading& A constant reference to the altitude reading.
 */
const Reading &GetAltitude() { return altitude; }

}  // namespace Climate
