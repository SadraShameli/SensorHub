#pragma once
#include <vector>

#define IPV4_LEN 4 * 4 + 1
#define MAC_LEN 6 * 3 + 1

class WiFi
{
public:
    struct ClientDetails
    {
        char IPAddress[IPV4_LEN];
        char MacAddress[MAC_LEN];
    };

    const static int MaxClientCount = 4;

public:
    static void Init();
    static void StartAP();
    static void StartStation();
    static bool IsConnected();

    static void SetMacAddress();

    static const std::string &GetIPAP();
    static const std::string &GetIPStation();
    static const std::string &GetMacAddress();
    static const std::vector<ClientDetails> GetClientDetails();

private:
    inline static std::string m_MacAddress;
};