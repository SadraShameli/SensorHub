#pragma once

#include "core/Service.h"

namespace Gui {

extern const Kernel::Service kService;

void Init();
void Update();

void Pause();
void Resume();

}
