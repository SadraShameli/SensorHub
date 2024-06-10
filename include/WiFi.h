#pragma once
#include <string>
#include <vector>

namespace WiFi
{
    namespace Constants
    {
        static const uint32_t IPV4Length = 4 * 4 + 1,
                              MACLength = 6 * 3 + 1,
                              MaxRetries = 10,
                              MaxClients = 4;
    };

    enum States
    {
        Connected = 1,
        Failed,
    };

    struct ClientDetails
    {
        char IPAddress[Constants::IPV4Length];
        char MacAddress[Constants::MACLength];
    };

    void Init();

    void StartAP();
    void StartStation();
    bool IsConnected();
    void WaitForConnection();

    void SetMacAddress();
    const std::string &GetIPAP();
    const std::vector<ClientDetails> GetClientDetails();

    const std::string &GetIPStation();
    const std::string &GetMacAddress();
};