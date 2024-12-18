#include "Display.h"

#include <ctime>

#include "Climate.h"
#include "Configuration.h"
#include "Gui.h"
#include "Mic.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "ssd1306.h"

namespace Display {
namespace Constants {

static const uint32_t LogoDuration = 1000, ScreenSaverDuration = 1 * 60 * 1000;

};

static const char* TAG = "Display";

static ssd1306_handle_t dev = nullptr;
static Configuration::Menu::Menus currentMenu = Configuration::Menu::Main;
static clock_t currentTime = 0, prevTime = 0;
static bool displayOff = false, isOK = false;

/**
 * @brief Initializes the display and I2C configuration.
 *
 * This function sets up the I2C configuration for the display, installs the I2C
 * driver, and initializes the SSD1306 display. If the display is not detected,
 * the task is deleted. It also clears the display, prints initial messages, and
 * refreshes the display. If the device is in configuration mode, it sets the
 * current menu to the configuration menu.
 *
 * @note This function uses ESP-IDF logging and error checking macros.
 */
void Init() {
    ESP_LOGI(TAG, "Initializing");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master =
            {
                .clk_speed = 1000000,
            },
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    dev = ssd1306_create(I2C_NUM_0, 0x3c);
    if (dev == nullptr) {
        ESP_LOGI(TAG, "No display detected");

        vTaskDelete(nullptr);
    }

    isOK = true;

    Clear();

    Print(35, 20, ">_sadra");
    Print(30, 40, "Sensor Hub");
    Refresh();
    vTaskDelay(pdMS_TO_TICKS(Constants::LogoDuration));

    Clear();

    if (Storage::GetConfigMode()) {
        currentMenu = Configuration::Menu::Config;
    }
}

/**
 * @brief Updates the display state based on the elapsed time.
 *
 * This function checks the elapsed time since the last update and turns off the
 * display if the elapsed time exceeds the screen saver duration. It also pauses
 * the GUI.
 */
void Update() {
    currentTime = clock();

    if ((currentTime - prevTime) > Constants::ScreenSaverDuration) {
        prevTime = currentTime;
        displayOff = true;

        ssd1306_display_off(dev);
        Gui::Pause();
    }
}

/**
 * @brief Checks if the system is in an OK state.
 *
 * @return `true` if the system is OK, `false` otherwise.
 */
bool IsOK() {
    return isOK;
}

/**
 * @brief Clears the display screen.
 *
 * This function clears the entire screen of the SSD1306 display by setting all
 * pixels to 0.
 */
void Clear() {
    ssd1306_clear_screen(dev, 0x00);
}

/**
 * @brief Refreshes the display by updating the graphical memory.
 *
 * This function calls the `ssd1306_refresh_gram` function to refresh the
 * display with the current graphical memory content.
 */
void Refresh() {
    ssd1306_refresh_gram(dev);
}

/**
 * @brief Resets the screen saver and turns the display back on.
 *
 * This function checks if the display is currently off. If it is, it resets the
 * screen saver by updating the previous time to the current clock time, sets
 * the `displayOff` flag to false, turns the display on, and resumes the GUI.
 */
void ResetScreenSaver() {
    if (!displayOff) {
        return;
    }

    prevTime = clock();
    displayOff = false;

    ssd1306_display_on(dev);
    Gui::Resume();
}

/**
 * @brief Prints a string on the display at the specified coordinates.
 *
 * @param x The x-coordinate where the text will be printed.
 * @param y The y-coordinate where the text will be printed.
 * @param text The string to be printed on the display.
 * @param size The size of the text to be printed.
 */
void Print(uint32_t x, uint32_t y, const char* text, uint32_t size) {
    ssd1306_draw_string(dev,
                        (uint8_t)x,
                        (uint8_t)y,
                        (const uint8_t*)text,
                        (uint8_t)size,
                        1);
}

/**
 * @brief Prints a header and a message on the display.
 *
 * This function clears the display, prints the header at the top,
 * prints the message below the header, and then refreshes the display.
 *
 * @param header The header text to be printed at the top of the display.
 * @param message The message text to be printed below the header.
 */
void PrintText(const char* header, const char* message) {
    Clear();

    Print(0, 0, header);
    Print(0, 16, message);

    Refresh();
}

/**
 * @brief Prints four lines of text on the display.
 *
 * This function clears the display, prints four lines of text at specified
 * positions, and then refreshes the display to show the new content.
 *
 * @param line1 The text to print on the first line.
 * @param line2 The text to print on the second line.
 * @param line3 The text to print on the third line.
 * @param line4 The text to print on the fourth line.
 */
void PrintLines(const char* line1, const char* line2, const char* line3,
                const char* line4) {
    Clear();

    Print(0, 0, line1);
    Print(0, 16, line2);
    Print(0, 32, line3);
    Print(0, 48, line4);

    Refresh();
}

/**
 * @brief Prints the main display information including device name, IP address,
 *        temperature, humidity, and loudness.
 *
 * This function retrieves and displays various sensor readings and device
 * information on the display. It shows the device name, IP address (if
 * connected to WiFi), temperature, humidity, and loudness levels. The display
 * is cleared before printing the new information and refreshed at the end.
 */
void PrintMain() {
    const std::string &deviceName = Storage::GetDeviceName(),
                      &ip = WiFi::GetIPStation();

    const Reading &temperature = Climate::GetTemperature(),
                  &humidity = Climate::GetHumidity(),
                  &loudness = Mic::GetLoudness();

    char buff[32] = {0};
    Clear();

    Print(0, 0, deviceName.c_str());

    if (WiFi::IsConnected()) {
        Print(0, 13, ("IP: " + ip).c_str());
    } else {
        Print(0, 13, "Connecting to WiFi");
    }

    if (Climate::IsOK()) {
        if (Storage::GetSensorState(Configuration::Sensor::Temperature)) {
            sprintf(buff, "Temperature: %dc", (int)temperature.Current());
            Print(0, 26, buff);
        }

        if (Storage::GetSensorState(Configuration::Sensor::Humidity)) {
            sprintf(buff, "Humidity: %d%%", (int)humidity.Current());
            Print(0, 39, buff);
        }
    }

    if (Mic::IsOK()) {
        if (Storage::GetSensorState(Configuration::Sensor::Recording) ||
            Storage::GetSensorState(Configuration::Sensor::Loudness)) {
            sprintf(buff, "Loudness: %ddB", (int)loudness.Current());
            Print(0, 52, buff);
        }
    }

    Refresh();
}

/**
 * @brief Prints the current, maximum, and minimum temperature readings on the
 * display.
 *
 * This function retrieves the current temperature reading from the Climate
 * sensor, formats the temperature values, and prints them on the display at
 * specified positions. It clears the display before printing and refreshes the
 * display after printing.
 */
void PrintTemperature() {
    const Reading& reading = Climate::GetTemperature();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Temperature");

    sprintf(buff, "%dc", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %dc", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %dc", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the current, maximum, and minimum humidity readings on the
 * display.
 *
 * This function retrieves the current humidity reading from the Climate sensor,
 * formats the readings into strings, and prints them on the display at
 * specified positions. It first clears the display, prints the "Humidity"
 * label, and then prints the current, maximum, and minimum humidity values.
 * Finally, it refreshes the display to show the updated information.
 */
void PrintHumidity() {
    const Reading& reading = Climate::GetHumidity();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Humidity");

    sprintf(buff, "%d%%", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %d%%", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %d%%", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the current, maximum, and minimum air pressure readings to the
 * display.
 *
 * This function retrieves the current air pressure reading from the Climate
 * sensor, formats the readings, and prints them to the display at specified
 * positions. The display is cleared before printing the new readings, and
 * refreshed after printing.
 */
void PrintAirPressure() {
    const Reading& reading = Climate::GetAirPressure();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Air Pressure");

    sprintf(buff, "%d hPa", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %d hPa", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %d hPa", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the gas resistance readings on the display.
 *
 * This function retrieves the current, maximum, and minimum gas resistance
 * readings from the Climate sensor and displays them on the screen. The
 * readings are displayed in ohms at different positions on the screen.
 *
 * The display is cleared before printing the new readings, and the screen
 * is refreshed after all the readings are printed.
 */
void PrintGasResistance() {
    const Reading& reading = Climate::GetGasResistance();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Gas Resistance");

    sprintf(buff, "%d Ohms", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %d Ohms", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %d Ohms", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the current, maximum, and minimum altitude readings on the
 * display.
 *
 * This function retrieves the current altitude reading from the Climate sensor,
 * formats the readings into strings, and prints them on the display at
 * specified positions. It first clears the display, prints the "Altitude"
 * label, and then prints the current altitude, maximum altitude, and minimum
 * altitude at different positions on the display. Finally, it refreshes the
 * display to show the updated information.
 */
void PrintAltitude() {
    const Reading& reading = Climate::GetAltitude();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Altitude");

    sprintf(buff, "%dm", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %dm", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %dm", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the loudness readings from the microphone to the display.
 *
 * This function retrieves the current, maximum, and minimum loudness readings
 * from the microphone and displays them on the screen. The loudness values are
 * displayed in decibels (dB) at different positions on the screen.
 *
 * The display is cleared before printing the new values, and the screen is
 * refreshed at the end to ensure the new values are shown.
 */
void PrintLoudness() {
    const Reading& reading = Mic::GetLoudness();

    char buff[32] = {0};
    Clear();

    Print(0, 0, "Loudness");

    sprintf(buff, "%ddB", (int)reading.Current());
    Print(0, 16, buff);

    sprintf(buff, "Max: %ddB", (int)reading.Max());
    Print(0, 32, buff);

    sprintf(buff, "Min: %ddB", (int)reading.Min());
    Print(0, 48, buff);

    Refresh();
}

/**
 * @brief Prints the details of connected WiFi clients on the display.
 *
 * This function retrieves the list of connected WiFi clients and displays
 * their IP and MAC addresses on the screen. The display is cleared before
 * printing the details, and the screen is refreshed after printing.

 * @note The function assumes that the `WiFi::GetClientDetails()` function
 * returns a vector of `WiFi::ClientDetails` objects, each containing
 * `IPAddress` and `MacAddress` fields.
 */
void PrintWiFiClients() {
    const std::vector<WiFi::ClientDetails>& clients = WiFi::GetClientDetails();

    Clear();

    Print(0, 0, "Connected devices");

    uint32_t offset = 13;
    for (const auto& client : clients) {
        Print(0, offset, client.IPAddress);
        offset += 10;

        Print(0, offset, client.MacAddress);
        offset += 10;
    }

    Refresh();
}

/**
 * @brief Advances the current menu to the next menu based on the current state.
 *
 * This function updates the `currentMenu` variable to the next appropriate menu
 * depending on whether the system is in configuration mode or not. The function
 * handles different cases for each menu and transitions to the next logical
 * menu.
 *
 * If the system is in configuration mode (`Storage::GetConfigMode()` returns
 * true):
 *
 * - Transitions from `Config` to `ConfigClients`.
 *
 * - Transitions from `ConfigClients` to `Failsafe`.
 *
 * - Transitions from `Failsafe` to either `ConfigConnected` or
 * `ConfigConnecting` based on the notification and WiFi connection status.
 *
 * - Transitions from `ConfigConnecting` or `ConfigConnected` back to
 * `Failsafe`.
 *
 * If the system is not in configuration mode:
 *
 * - Transitions from `Failsafe` or `Reset` to `Main`.
 *
 * - Iterates through the sensor menus and transitions to the first active
 * sensor menu that meets the conditions for climate or microphone status.
 *
 * - Defaults to `Failsafe` if no other menu is appropriate.
 *
 * @note The function assumes that `currentMenu` is a valid menu state and that
 *       `Menus` and `Sensors` enums are properly defined in the `Configuration`
 * namespace.
 */
void NextMenu() {
    using Menus = Configuration::Menu::Menus;
    using Sensors = Configuration::Sensor::Sensors;

    if (Storage::GetConfigMode()) {
        switch (currentMenu) {
            case Menus::Config:
                currentMenu = Menus::ConfigClients;
                return;

            case Menus::ConfigClients:
                currentMenu = Menus::Failsafe;
                return;

            case Menus::Failsafe:
                if (Configuration::Notification::Get(
                        Configuration::Notification::ConfigSet)) {
                    if (WiFi::IsConnected()) {
                        currentMenu = Menus::ConfigConnected;
                    } else {
                        currentMenu = Menus::ConfigConnecting;
                    }

                    return;
                }

                currentMenu = Menus::Config;
                return;

            case Menus::ConfigConnecting:
            case Menus::ConfigConnected:
                currentMenu = Menus::Failsafe;
                return;

            default:
                return;
        }
    }

    else {
        switch (currentMenu) {
            case Menus::Failsafe:
            case Menus::Reset:
                currentMenu = Menus::Main;
                return;

            default:
                for (int i = currentMenu + 1; i < Menus::Failsafe; i++) {
                    if (Storage::GetSensorState((Sensors)i)) {
                        if (Climate::IsOK() && (i >= Sensors::Temperature &&
                                                i <= Sensors::Altitude)) {
                            currentMenu = (Menus)i;
                            return;
                        }

                        if (Mic::IsOK() && (i >= Sensors::Loudness ||
                                            i <= Sensors::Recording)) {
                            currentMenu = (Menus)i;
                            return;
                        }
                    }
                }

                currentMenu = Menus::Failsafe;
        }
    }
}

/**
 * @brief Retrieves the current menu configuration.
 *
 * @return `Configuration::Menu::Menus` The current menu.
 */
Configuration::Menu::Menus GetMenu() {
    return currentMenu;
}

/**
 * @brief Sets the current menu and resets the screen saver.
 *
 * This function updates the current menu to the specified menu and
 * resets the screen saver timer to prevent the screen saver from
 * activating immediately after the menu change.
 *
 * @param menu The menu to set as the current menu. This should be one
 *             of the values from the `Configuration::Menu::Menus` enum.
 */
void SetMenu(Configuration::Menu::Menus menu) {
    currentMenu = menu;

    ResetScreenSaver();
}

}  // namespace Display
