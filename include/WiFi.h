#pragma once

#include <string>
#include <vector>

#include "core/Service.h"

namespace WiFi {

extern const Kernel::Service kService;

namespace Constants {

static const uint32_t IPv4Length = 4 * 4 + 1, MacLength = 6 * 3 + 1,
                      MaxRetries = 10, MaxClients = 4;

};

enum States {
    Connected = 1 << 0,
    ConnectFailed = 1 << 1,
};

struct ClientDetails {
    char IPAddress[Constants::IPv4Length];
    char MacAddress[Constants::MacLength];
};

void Init();

void StartAP();
void StartStation();
bool IsConnected();
void WaitForConnection();
bool WaitForConnection(uint32_t timeoutMs);
int GetLastDisconnectReason();
const char* DisconnectReasonString(int reason);

void SetMacAddress();
const std::string& GetIPAP();
const std::string& GetIPStation();
const std::string& GetMacAddress();
const std::vector<ClientDetails>& GetClientDetails();

};