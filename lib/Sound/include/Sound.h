#pragma once

class Sound
{
public:
    static void Init();
    static void Update();

    static double GetLevel();
    static double GetMaxLevel();
    static double GetMinLevel();
    static void ResetLevels();

private:
    inline static double m_SoundLevel = 0.0f;
    inline static double m_MaxLevel = 0.0f;
    inline static double m_MinLevel = 120.0f;
};