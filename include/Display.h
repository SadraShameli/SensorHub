#pragma once

#include "Configuration.h"
#include "WiFi.h"
#include "sensors/ISensor.h"

namespace Display {

void Init();
void Update();
bool IsOK();

void Clear();
void Refresh();
void ResetScreenSaver();

void Print(uint32_t, uint32_t, const char*, uint32_t = 12);
void PrintText(const char*, const char*);
void PrintLines(const char*, const char*, const char*, const char*);

void PrintMain();
void PrintSensorMenu(const Sensors::ISensor& sensor);
void PrintWiFiClients();

void NextMenu();
Configuration::Menu::Menus GetMenu();
void SetMenu(Configuration::Menu::Menus);

}
