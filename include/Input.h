#pragma once

/**
 * @namespace
 * @brief A namespace for managing the input.
 */
namespace Input {

enum Inputs { Up = 27, Down = 26 };

void Init();
void Update();

bool GetPinState(Inputs);

};  // namespace Input
