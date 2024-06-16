#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "Storage.h"

namespace Storage
{
    namespace Constants
    {
        static const uint32_t SSIDLength = 33,
                              PasswordLength = 65,
                              UUIDLength = 37,
                              EndpointLength = 241;
    };

    struct StorageData
    {
        uint32_t SSID[Constants::SSIDLength];
        uint32_t Password[Constants::PasswordLength];
        uint32_t Address[Constants::EndpointLength];
        uint32_t AuthKey[Constants::UUIDLength];
        uint32_t DeviceName[Constants::UUIDLength];
        uint32_t DeviceId;
        uint32_t LoudnessThreshold;
        uint32_t RegisterInterval;
        bool Sensors[Configuration::Sensor::SensorCount - 1];
        bool ConfigMode;
    };

    static const char *TAG = "Storage";
    static nvs_handle_t nvsHandle = 0;

    static uint32_t encryptionMask = 0;
    static StorageData storageData = {0};
    static std::string ssid, password, deviceName, address, AuthKey;

    void Init()
    {
        ESP_LOGI(TAG, "Initializing");

        size_t required_size = 0;
        storageData.ConfigMode = true;

        CalculateMask();

        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_LOGI(TAG, "Initialization failed");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        ESP_ERROR_CHECK(err);
        ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &nvsHandle));

        ESP_LOGI(TAG, "Reading data");
        ESP_ERROR_CHECK(nvs_get_blob(nvsHandle, TAG, nullptr, &required_size));
        ESP_ERROR_CHECK(nvs_get_blob(nvsHandle, TAG, &storageData, &required_size));

        ESP_LOGI(TAG, "Config mode: %s", storageData.ConfigMode == true ? "true" : "false");
        if (!storageData.ConfigMode)
        {
            ssid.reserve(Constants::SSIDLength);
            DecryptText(storageData.SSID, ssid);
            ESP_LOGI(TAG, "Decrypted SSID: %s", ssid.c_str());

            password.reserve(Constants::PasswordLength);
            DecryptText(storageData.Password, password);
            ESP_LOGI(TAG, "Decrypted Password: %s", password.c_str());

            address.reserve(Constants::EndpointLength);
            DecryptText(storageData.Address, address);
            ESP_LOGI(TAG, "Decrypted Address: %s", address.c_str());

            AuthKey.reserve(Constants::UUIDLength);
            DecryptText(storageData.AuthKey, AuthKey);
            ESP_LOGI(TAG, "Decrypted Auth Key: %s", AuthKey.c_str());

            deviceName.reserve(Constants::UUIDLength);
            DecryptText(storageData.DeviceName, deviceName);
            ESP_LOGI(TAG, "Decrypted Device Name: %s", deviceName.c_str());

            ESP_LOGI(TAG, "Device Id: %ld", storageData.DeviceId);
            ESP_LOGI(TAG, "Loudness Threshold: %ld", storageData.LoudnessThreshold);
            ESP_LOGI(TAG, "Register Interval: %ld", storageData.RegisterInterval);

            for (int i = 0; i < Configuration::Sensor::SensorCount - 1; i++)
                ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1, storageData.Sensors[i] ? "enabled" : "disabled");
        }
    }

    void Commit()
    {
        if (ssid.length() > Constants::SSIDLength)
        {
            Failsafe::AddFailure(TAG, "SSID too long");
            return;
        }

        if (password.length() > Constants::PasswordLength)
        {
            Failsafe::AddFailure(TAG, "Password too long");
            return;
        }

        if (address.length() > Constants::EndpointLength)
        {
            Failsafe::AddFailure(TAG, "Address too long");
            return;
        }

        if (AuthKey.length() > Constants::UUIDLength)
        {
            Failsafe::AddFailure(TAG, "Auth Key too long");
            return;
        }

        if (deviceName.length() > Constants::UUIDLength)
        {
            Failsafe::AddFailure(TAG, "Device Name too long");
            return;
        }

        ESP_LOGI(TAG, "Encrypting SSID: %s", ssid.c_str());
        EncryptText(storageData.SSID, ssid);

        ESP_LOGI(TAG, "Encrypting Password: %s", password.c_str());
        EncryptText(storageData.Password, password);

        ESP_LOGI(TAG, "Encrypting Address: %s", address.c_str());
        EncryptText(storageData.Address, address);

        ESP_LOGI(TAG, "Encrypting Auth Key: %s", AuthKey.c_str());
        EncryptText(storageData.AuthKey, AuthKey);

        ESP_LOGI(TAG, "Encrypting Device Name: %s", deviceName.c_str());
        EncryptText(storageData.DeviceName, deviceName);

        ESP_LOGI(TAG, "Device Id: %ld", storageData.DeviceId);
        ESP_LOGI(TAG, "Loudness Threshold: %ld", storageData.LoudnessThreshold);
        ESP_LOGI(TAG, "Register Interval: %ld", storageData.RegisterInterval);

        for (int i = 0; i < (Configuration::Sensor::SensorCount - 1); i++)
            ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1, storageData.Sensors[i] ? "enabled" : "disabled");

        ESP_LOGI(TAG, "Saving data");
        ESP_ERROR_CHECK(nvs_set_blob(nvsHandle, TAG, &storageData, sizeof(StorageData)));
        ESP_ERROR_CHECK(nvs_commit(nvsHandle));
    }

    void Reset()
    {
        ESP_LOGI(TAG, "Resetting data");

        storageData = {0};
        storageData.ConfigMode = true;

        ESP_ERROR_CHECK(nvs_set_blob(nvsHandle, TAG, &storageData, sizeof(StorageData)));
        ESP_ERROR_CHECK(nvs_commit(nvsHandle));
    }

    void Mount(const char *base_path, const char *partition_label)
    {
        ESP_LOGI(TAG, "Mounting partition %s - base path: %s", base_path, partition_label);

        esp_vfs_spiffs_conf_t storage_config = {
            .base_path = base_path,
            .partition_label = partition_label,
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        ESP_ERROR_CHECK(esp_vfs_spiffs_register(&storage_config));

#ifdef UNIT_DEBUG
        ESP_LOGI(TAG, "Performing check");
        ESP_ERROR_CHECK(esp_spiffs_check(partition_label));
#endif

        size_t total = 0, used = 0;
        esp_err_t err = esp_spiffs_info(storage_config.partition_label, &total, &used);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Getting partition information failed: %s - formatting", esp_err_to_name(err));
            err = esp_spiffs_format(storage_config.partition_label);
            ESP_ERROR_CHECK(err);
        }
        ESP_LOGI(TAG, "Partition info: total: %u - used: %u", total, used);
    }

    void CalculateMask()
    {
        ESP_LOGI(TAG, "Calculating encryption mask");

        uint32_t maskArray[10] = {0};
        uint32_t macArray[10] = {0};
        uint32_t mask = 1564230594;
        uint64_t mask2 = 0;

        union
        {
            uint64_t NumberRepresentation;
            uint8_t ArrayRepresentation[6];
        } mac = {0};

        ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac.ArrayRepresentation));
        mask2 = mac.NumberRepresentation;

        for (int i = 10 - 1; i >= 0; i--)
        {
            macArray[i] = mask2 % 10;
            mask2 /= 10;
        }

        for (int i = 10 - 1; i >= 0; i--)
        {
            maskArray[i] = mask % 10;
            mask /= 10;
        }

        for (int i = 0; i <= 10 - 1; i += 2)
        {
            encryptionMask *= 10;
            encryptionMask += macArray[i];
        }

        for (int i = 9; i >= 1; i -= 2)
        {
            encryptionMask *= 10;
            encryptionMask += maskArray[i];
        }
    }

    void EncryptText(uint32_t *var, const std::string &str)
    {
        for (const char c : str)
            *(var++) = c ^ encryptionMask;
    }

    void DecryptText(uint32_t *var, std::string &str)
    {
        str.clear();
        while (*var)
            str.push_back(*(var++) ^ encryptionMask);
    }

    const std::string &GetSSID() { return ssid; }
    const std::string &GetPassword() { return password; }
    const std::string &GetAddress() { return address; }
    const std::string &GetAuthKey() { return AuthKey; }
    const std::string &GetDeviceName() { return deviceName; }
    uint32_t GetDeviceId() { return storageData.DeviceId; }
    uint32_t GetLoudnessThreshold() { return storageData.LoudnessThreshold; }
    uint32_t GetRegisterInterval() { return storageData.RegisterInterval; }
    bool GetSensorState(Configuration::Sensor::Sensors sensor) { return storageData.Sensors[sensor - 1]; }
    bool GetConfigMode() { return storageData.ConfigMode; };

    void SetSSID(std::string &&str) { ssid = std::move(str); }
    void SetPassword(std::string &&str) { password = std::move(str); }
    void SetAddress(std::string &&str) { address = std::move(str); }
    void SetAuthKey(std::string &&str) { AuthKey = std::move(str); }
    void SetDeviceName(std::string &&str) { deviceName = std::move(str); }
    void SetDeviceId(uint32_t num) { storageData.DeviceId = num; }
    void SetLoudnessThreshold(uint32_t num) { storageData.LoudnessThreshold = num; }
    void SetRegisterInterval(uint32_t num) { storageData.RegisterInterval = num; }
    void SetSensorState(Configuration::Sensor::Sensors sensor, bool state) { storageData.Sensors[sensor - 1] = state; }
    void SetConfigMode(bool config) { storageData.ConfigMode = config; }
}
