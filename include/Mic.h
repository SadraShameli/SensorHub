#pragma once

#include "Definitions.h"

/**
 * @namespace
 * @brief A namespace for managing the microphone.
 */
namespace Mic {

void Init();
void Update();

void UpdateRecording();
bool UpdateLoudness();
void RegisterRecordings();
float CalculateLoudness();

bool IsOK();
void ResetValues();
const Reading &GetLoudness();

};  // namespace Mic