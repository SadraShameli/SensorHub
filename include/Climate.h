#pragma once
#include "Definitions.h"
#include "Configuration.h"

class Climate
{
public:
    static void Init();
    static void Update();
    static bool IsOK();

    static void ResetValues(Configuration::Sensors::Sensor);
    static const Reading &GetTemperature() { return m_Temperature; }
    static const Reading &GetHumidity() { return m_Humidity; }
    static const Reading &GetAirPressure() { return m_AirPressure; }
    static const Reading &GetGasResistance() { return m_GasResistance; }
    static const Reading &GetAltitude() { return m_Altitude; }

private:
    inline static Reading m_Temperature, m_Humidity, m_AirPressure, m_GasResistance, m_Altitude;
    struct Constants
    {
        static constexpr float TemperatureOffset = -2,
                               HumidityOffset = 13,
                               AirPressureOffset = -12,
                               GasResistanceOffset = 0,
                               AltitudeOffset = 0,
                               SeaLevelPressure = 1021, SeaLevelTemperature = 14;
    };
};