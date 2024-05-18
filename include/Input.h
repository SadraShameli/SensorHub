#pragma once
#include "Backend.h"
#include "Pin.h"

class Input
{
public:
    enum Inputs
    {
        Up = 27,
        Down = 26
    };

    static void Init();
    static void Update();
    static bool GetPinState(Inputs);
};
