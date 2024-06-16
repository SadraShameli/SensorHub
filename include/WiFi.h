#pragma once
#include <string>
#include <vector>

namespace WiFi
{
    namespace Constants
    {
        static const uint32_t IPv4Length = 4 * 4 + 1,
                              MacLength = 6 * 3 + 1,
                              MaxRetries = 10,
                              MaxClients = 4;
    };

    enum States
    {
        Connected = 1,
    };

    struct ClientDetails
    {
        char IPAddress[Constants::IPv4Length];
        char MacAddress[Constants::MacLength];
    };

    void Init();

    void StartAP();
    void StartStation();
    bool IsConnected();
    void WaitForConnection();

    void SetMacAddress();
    const std::string &GetIPAP();
    const std::string &GetIPStation();
    const std::string &GetMacAddress();
    const std::vector<ClientDetails> &GetClientDetails();
};