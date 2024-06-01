#pragma once
#include "Definitions.h"

class Sound
{
public:
    static void Init();
    static void Update();
    static void UpdateRecording();
    static bool IsOK();

    static bool ReadSound();
    static void RegisterRecordings();

    static void ResetLevels() { m_Loudness.Reset(); };
    static const Reading &GetLoudness() { return m_Loudness; }

private:
    inline static Reading m_Loudness;
    struct Constants
    {
        static constexpr float LoudnessOffset = 0;
    };
};