#pragma once

/**
 * @namespace
 * @brief A namespace for managing the network.
 */
namespace Network {

void Init();
void Update();
void UpdateConfig();
void Reset();

void NotifyConfigSet();

};  // namespace Network