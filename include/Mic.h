#pragma once

#include "Definitions.h"
#include "core/Service.h"

namespace Mic {

extern const Kernel::Service kService;
extern const Kernel::Service kSenderService;

void Init();
void Update();

void UpdateRecording();
bool UpdateLoudness();
float CalculateLoudness();

bool IsOK();
void ResetValues();
const Reading& GetLoudness();

};