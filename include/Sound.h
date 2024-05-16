#pragma once

class Sound
{
public:
    static void Init();
    static void Update();
    static bool ReadSound();
    static void RegisterRecordings();

    static double GetLevel() { return m_SoundLevel; };
    static double GetMaxLevel() { return m_MaxLevel; };
    static double GetMinLevel() { return m_MinLevel; };
    static void ResetLevels() { m_MaxLevel = m_MinLevel = m_SoundLevel; };

private:
    inline static double m_SoundLevel = 0.0f;
    inline static double m_MaxLevel = 0.0f;
    inline static double m_MinLevel = 120.0f;
};