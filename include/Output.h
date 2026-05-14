#pragma once

#include <cstdint>

namespace Output {

enum Outputs { LedR = 32, LedY = 33, LedG = 25 };

void Init();
void Update();

void Toggle(Outputs, bool);

void Blink(Outputs, int64_t blinkTimeMs = 50, bool continuous = false);

void SetContinuity(Outputs, bool);

}
