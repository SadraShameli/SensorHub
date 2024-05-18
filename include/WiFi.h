#pragma once
#include <vector>

#define IPV4_LEN 4 * 4 + 1
#define MAC_LEN 6 * 3 + 1

class WiFi
{
public:
    enum State
    {
        Connected = 1,
        Failed,
    };

    struct ClientDetails
    {
        char IPAddress[IPV4_LEN];
        char MacAddress[MAC_LEN];
    };

    static const int MaxClientCount = 4;

public:
    static void Init();
    static void StartAP();
    static void StartStation();
    static bool IsConnected();
    static void WaitForConnection();

    static void SetMacAddress();
    static const std::string &GetIPAP();
    static const std::vector<ClientDetails> GetClientDetails();

    static const std::string &GetIPStation();
    static const std::string &GetMacAddress() { return m_MacAddress; };

private:
    inline static std::string m_MacAddress;
};