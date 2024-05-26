#pragma once
#include "Definitions.h"
#include "Configuration.h"

class Climate
{
public:
    static void Init();
    static void Update();
    static bool Initialized();

    static void ResetValues(Configuration::Sensors::Sensor);
    static const Reading &GetTemperature() { return m_Temperature; }
    static const Reading &GetHumidity() { return m_Humidity; }
    static const Reading &GetAirPressure() { return m_AirPressure; }
    static const Reading &GetGasResistance() { return m_GasResistance; }
    static const Reading &GetAltitude() { return m_Altitude; }

private:
    inline static Reading m_Temperature = {0, 150, 0};
    inline static Reading m_Humidity = {0, 100, 0};
    inline static Reading m_AirPressure = {0, 1300, 0};
    inline static Reading m_GasResistance = {0, 1000000, 0};
    inline static Reading m_Altitude = {0, 100000, 0};

    static const int TemperatureOffset = -9;
    static const int HumidityOffset = 5;
    static const int AirPressureOffset = 1;
    static const int GasResistanceOffset = 0;
    static const int AltitudeOffset = 15;
    static const int SeaLevelPressure = 1011;
};