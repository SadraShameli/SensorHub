#pragma once
#include "Definitions.h"

namespace Mic
{
    void Init();
    void Update();

    void UpdateRecording();
    bool UpdateLoudness();
    void RegisterRecordings();
    float CalculateLoudness();

    bool IsOK();
    void ResetValues();
    const Reading &GetLoudness();
};