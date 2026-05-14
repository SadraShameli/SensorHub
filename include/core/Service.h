#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace Kernel {

enum RunMode : uint8_t {
    RunInConfigMode = 1u << 0,
    RunInNormalMode = 1u << 1,
    RunAlways = RunInConfigMode | RunInNormalMode,
};

struct Service {

    const char* name;

    uint8_t modes;

    void (*on_init)();

    void (*task_entry)(void*);
    uint32_t stack_bytes;
    UBaseType_t priority;

    TaskHandle_t* out_handle;

    bool (*should_start)();
};

}
