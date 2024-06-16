#pragma once
#include "Configuration.h"
#include "WiFi.h"

namespace Display
{
    void Init();
    void Update();

    void Clear();
    void Refresh();
    void ResetScreenSaver();

    void Print(uint8_t, uint8_t, const char *, uint8_t = 12);
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
    // void PrintRPM();

    void NextMenu();

    bool IsOK();
    Configuration::Menu::Menus GetMenu();
    void SetMenu(Configuration::Menu::Menus);
};