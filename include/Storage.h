#pragma once
#include "Backend.h"

class Storage
{
public:
    static void Init();
    static void MountSPIFFS(const char *, const char *);

    static void Commit();
    static void Reset();

    static void GetSSID(std::string &);
    static void GetPassword(std::string &);
    static void GetDeviceName(std::string &);
    static void GetAuthKey(std::string &);
    static void GetAddress(std::string &);
    static void GetDeviceId(uint32_t &);
    static void GetLoudnessThreshold(uint32_t &);
    static void GetRegisterInterval(uint32_t &);
    static bool GetEnabledSensors(Backend::SensorTypes);
    static bool GetConfigMode();

    static void SetSSID(const std::string &);
    static void SetPassword(const std::string &);
    static void SetDeviceName(const std::string &);
    static void SetAuthKey(const std::string &);
    static void SetAddress(const std::string &);
    static void SetDeviceId(uint32_t);
    static void SetLoudnessThreshold(uint32_t);
    static void SetRegisterInterval(uint32_t);
    static void SetEnabledSensors(Backend::SensorTypes, bool);
    static void SetConfigMode(bool);

private:
    static void CalculateMask();
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
        uint32_t DeviceName[UUIDLength];
        uint32_t AuthKey[UUIDLength];
        uint32_t Address[EndpointLength];
        uint32_t DeviceId;
        uint32_t LoudnessThreshold;
        uint32_t RegisterInterval;
        bool EnabledSensors[Backend::SensorTypes::SensorCount - 1];
        bool ConfigMode;
    };

    inline static StorageData m_StorageData = {};
};