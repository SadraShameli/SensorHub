#pragma once
#include <string>
#include "Configuration.h"

class Storage
{
public:
    static void MountSPIFFS(const char *, const char *);
    static void Init();
    static void Commit();
    static void Reset();

    static const std::string &GetSSID() { return m_SSID; }
    static const std::string &GetPassword() { return m_Password; }
    static const std::string &GetAddress() { return m_Address; }
    static const std::string &GetAuthKey() { return m_AuthKey; }
    static const std::string &GetDeviceName() { return m_DeviceName; }
    static uint32_t GetDeviceId() { return m_StorageData.DeviceId; }
    static uint32_t GetLoudnessThreshold() { return m_StorageData.LoudnessThreshold; }
    static uint32_t GetRegisterInterval() { return m_StorageData.RegisterInterval; }
    static bool GetSensorState(Configuration::Sensors::Sensor sensor) { return m_StorageData.Sensors[(int)sensor - 1]; }
    static bool GetConfigMode() { return m_StorageData.ConfigMode; };

    static void SetSSID(std::string &&str) { m_SSID = str; }
    static void SetPassword(std::string &&str) { m_Address = str; }
    static void SetAddress(std::string &&str) { m_Address = str; }
    static void SetAuthKey(std::string &&str) { m_AuthKey = str; }
    static void SetDeviceName(std::string &&str) { m_DeviceName = str; }
    static void SetDeviceId(uint32_t num) { m_StorageData.DeviceId = num; }
    static void SetLoudnessThreshold(uint32_t num) { m_StorageData.LoudnessThreshold = num; }
    static void SetRegisterInterval(uint32_t num) { m_StorageData.RegisterInterval = num; }
    static void SetSensorState(Configuration::Sensors::Sensor sensor, bool state) { m_StorageData.Sensors[(int)sensor - 1] = state; }
    static void SetConfigMode(bool config) { m_StorageData.ConfigMode = config; }

private:
    static constexpr int SSIDLength = 33, PasswordLength = 65, UUIDLength = 37, EndpointLength = 241;
    struct StorageData
    {
        uint32_t SSID[SSIDLength];
        uint32_t Password[PasswordLength];
        uint32_t Address[EndpointLength];
        uint32_t AuthKey[UUIDLength];
        uint32_t DeviceName[UUIDLength];
        uint32_t DeviceId;
        uint32_t LoudnessThreshold;
        uint32_t RegisterInterval;
        bool Sensors[Configuration::Sensors::Sensor::SensorCount - 1];
        bool ConfigMode;
    };

    static void CalculateMask();
    static void EncryptText(uint32_t *, std::string &);
    static void DecryptText(uint32_t *, std::string &);

    inline static uint32_t m_EncryptionMask = 0;
    inline static StorageData m_StorageData = {};
    inline static std::string m_SSID, m_Password, m_DeviceName, m_Address, m_AuthKey;
};