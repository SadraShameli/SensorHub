#pragma once

namespace Input
{
    enum Inputs
    {
        Up = 27,
        Down = 26
    };

    void Init();
    void Update();

    bool GetPinState(Inputs);
};
