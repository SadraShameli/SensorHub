#include <string>
#include "Configuration.h"
#include "WiFi.h"

class Display
{
public:
    static void Init();
    static void Update();
    static void Clear();
    static void Refresh();
    static void ResetScreenSaver();
    static void Print(uint8_t, uint8_t, const char *, uint8_t = 12);

    static void NextMenu();
    static Configuration::Menus::Menu GetMenu() { return m_CurrentMenu; }
    static void SetMenu(Configuration::Menus::Menu menu) { m_CurrentMenu = menu; }

    static void PrintText(const char *, const char *);
    static void PrintLines(const char *, const char *, const char *, const char *);

    static void PrintMain();
    static void PrintTemperature();
    static void PrintHumidity();
    static void PrintAirPressure();
    static void PrintGasResistance();
    static void PrintAltitude();
    static void PrintLoudness();
    // static void PrintRPM();
    static void PrintWiFiClients();

private:
    static const uint32_t LogoDuration = 1000, ScreenSaverDuration = 1 * 60 * 1000;

    inline static Configuration::Menus::Menu m_CurrentMenu = Configuration::Menus::Main;
    inline static uint32_t m_CurrentTime = 0, m_PrevTime = 0;
};