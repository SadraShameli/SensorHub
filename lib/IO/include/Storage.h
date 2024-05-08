#pragma once
#include "Backend.h"

class Storage
{
public:
    static bool Init();
    static bool MountSPIFFS(const char *, const char *);

    static bool Commit();
    static bool Reset();

    static void GetSSID(std::string &);
    static void GetPassword(std::string &);
    static void GetDeviceId(std::string &);
    static void GetDeviceName(std::string &);
    static void GetAuthKey(std::string &);
    static void GetAddress(std::string &);
    static void GetLoudnessThreshold(int &);
    static void GetRegisterInterval(int &);
    static bool GetEnabledSensors(Backend::Menus);
    static bool GetConfigMode();

    static void SetSSID(const std::string &);
    static void SetPassword(const std::string &);
    static void SetDeviceId(const std::string &);
    static void SetDeviceName(const std::string &);
    static void SetAuthKey(const std::string &);
    static void SetAddress(const std::string &);
    static void SetLoudnessThreshold(int);
    static void SetRegisterInterval(int);
    static void SetEnabledSensors(Backend::Menus, bool);
    static void SetConfigMode(bool);

private:
    static bool CalculateMask();
    static void EncryptText(uint32_t *, const std::string &);
    static void DecryptText(uint32_t *, std::string &);

private:
    static constexpr int SSIDLength = 33, PasswordLength = 65;
    static constexpr int UUIDLength = 37, EndpointLength = 241;
    inline static uint32_t m_EncryptionMask = 0;

    struct StorageData
    {
        uint32_t SSID[SSIDLength];
        uint32_t Password[PasswordLength];
        uint32_t DeviceId[UUIDLength];
        uint32_t DeviceName[UUIDLength];
        uint32_t AuthKey[UUIDLength];
        uint32_t Address[EndpointLength];
        int LoudnessThreshold;
        int RegisterInterval;
        bool EnabledSensors[Backend::Menus::Config];
        bool ConfigMode;
    };

    inline static StorageData m_StorageData = {};
};