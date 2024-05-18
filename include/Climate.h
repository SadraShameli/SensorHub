#pragma once
#include "Backend.h"

class Climate
{
public:
    class Values
    {
    public:
        Values(float current, float min, float max) : m_Current(current), m_Min(min), m_Max(max) {}
        float GetCurrent() const { return m_Current; }
        float GetMin() const { return m_Min; }
        float GetMax() const { return m_Max; }
        void Reset() { m_Min = m_Max = m_Current; }
        void Update(float current)
        {
            m_Current = current;

            if ((current < m_Min) && current != 0)
            {

                m_Min = current;
            }

            if (current > m_Max)
            {
                m_Max = current;
            }
        }

        float m_Current, m_Min, m_Max;
    };

    static void Init();
    static void Update();
    static void ResetValues(Backend::SensorTypes);

    static const Values &GetTemperature() { return m_Temperature; }
    static const Values &GetHumidity() { return m_Humidity; }
    static const Values &GetAirPressure() { return m_AirPressure; }
    static const Values &GetGasResistance() { return m_GasResistance; }
    static const Values &GetAltitude() { return m_Altitude; }

private:
    inline static Values m_Temperature = {0, 0, 25};
    inline static Values m_Humidity = {0, 0, 100};
    inline static Values m_AirPressure = {0, 0, 1300};
    inline static Values m_GasResistance = {0, 0, 10000};
    inline static Values m_Altitude = {0, 0, 10000};

    static const int TemperatureOffset = -3;
    static const int HumidityOffset = 0;
    static const int AirPressureOffset = 0;
    static const int GasResistanceOffset = 0;
    static const int AltitudeOffset = 0;
    static const int SeaLevelPressure = 1013.25;
};