#pragma once

#include <string>
#include <vector>

/**
 * @namespace
 * @brief A namespace for managing the WiFi.
 */
namespace WiFi {

/**
 * @namespace
 * @brief A namespace for WiFi constants.
 */
namespace Constants {

static const uint32_t IPv4Length = 4 * 4 + 1, MacLength = 6 * 3 + 1,
                      MaxRetries = 10, MaxClients = 4;

};

/**
 * @enum
 * @brief Represents the states of the WiFi connection.
 */
enum States {
    Connected = 1,
};

/**
 * @struct
 * @brief Structure to hold the details of a WiFi client.
 */
struct ClientDetails {
    char IPAddress[Constants::IPv4Length];
    char MacAddress[Constants::MacLength];
};

void Init();

void StartAP();
void StartStation();
bool IsConnected();
void WaitForConnection();

void SetMacAddress();
const std::string& GetIPAP();
const std::string& GetIPStation();
const std::string& GetMacAddress();
const std::vector<ClientDetails>& GetClientDetails();

};  // namespace WiFi