#pragma once
#include "Definitions.h"

namespace Sound
{
    void Init();
    void Update();
    void UpdateRecording();

    double ReadLoudness();
    bool UpdateLoudness();
    void RegisterRecordings();

    bool IsOK();
    void ResetLevels();
    const Reading &GetLoudness();
};