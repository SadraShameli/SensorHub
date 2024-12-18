#pragma once

#include "Configuration.h"
#include "WiFi.h"

/**
 * @namespace
 * @brief A namespace for managing the display.
 */
namespace Display {

void Init();
void Update();
bool IsOK();

void Clear();
void Refresh();
void ResetScreenSaver();

void Print(uint32_t, uint32_t, const char *, uint32_t = 12);
void PrintText(const char *, const char *);
void PrintLines(const char *, const char *, const char *, const char *);

void PrintMain();
void PrintTemperature();
void PrintHumidity();
void PrintAirPressure();
void PrintGasResistance();
void PrintAltitude();
void PrintLoudness();
void PrintWiFiClients();

void NextMenu();
Configuration::Menu::Menus GetMenu();
void SetMenu(Configuration::Menu::Menus);

};  // namespace Display