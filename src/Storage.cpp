#include "Storage.h"

#include "Configuration.h"
#include "Failsafe.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace Storage {
namespace Constants {

static const uint32_t SSIDLength = 33, PasswordLength = 65, UUIDLength = 37,
                      EndpointLength = 241;
};

/**
 * @struct
 * @brief Represents the data stored in the non-volatile storage.
 */
struct StorageData {
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

static const char* TAG = "Storage";

static nvs_handle_t nvsHandle = 0;
static uint64_t encryptionMask = 0;
static StorageData storageData = {0};
static std::string ssid, password, deviceName, address, AuthKey;

/**
 * @brief Initializes the storage system and reads stored configuration data.
 *
 * This function performs the following steps:
 * 1. Logs the initialization process.
 *
 * 2. Sets the configuration mode to true.
 *
 * 3. Calculates the mask for storage data.
 *
 * 4. Initializes the NVS (Non-Volatile Storage) flash.
 *
 * 5. If the initialization fails due to no free pages or a new version found,
 * it erases the NVS flash and reinitializes it.
 *
 * 6. Opens the NVS handle for read and write operations.
 *
 * 7. Reads the stored data size and the actual data from NVS.
 *
 * 8. Logs the configuration mode status.
 *
 * 9. If not in configuration mode, decrypts and logs the SSID, password,
 * address, authentication key, device name, device ID, loudness threshold,
 * register interval, and sensor states.
 */
void Init() {
    ESP_LOGI(TAG, "Initializing");

    size_t required_size = 0;
    storageData.ConfigMode = true;

    CalculateMask();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Initialization failed");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &nvsHandle));

    ESP_LOGI(TAG, "Reading data");
    ESP_ERROR_CHECK(nvs_get_blob(nvsHandle, TAG, nullptr, &required_size));
    ESP_ERROR_CHECK(nvs_get_blob(nvsHandle, TAG, &storageData, &required_size));

    ESP_LOGI(TAG, "Config mode: %s",
             storageData.ConfigMode == true ? "true" : "false");

    if (!storageData.ConfigMode) {
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

        for (int i = 0; i < Configuration::Sensor::SensorCount - 1; i++) {
            ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1,
                     storageData.Sensors[i] ? "enabled" : "disabled");
        }
    }
}

/**
 * @brief Commits the configuration data to the non-volatile storage.
 *
 * This function performs the following steps:
 * 1. Checks if the SSID, password, address, authentication key, and device name
 * are not longer than the maximum allowed length.
 *
 * 2. Encrypts the SSID, password, address, authentication key, and device name.
 *
 * 3. Logs the encrypted SSID, password, address, authentication key, and device
 * name.
 *
 * 4. Logs the device ID, loudness threshold, register interval, and sensor
 * states.
 *
 * 5. Saves the data to the NVS flash.
 */
void Commit() {
    if (ssid.length() > Constants::SSIDLength) {
        Failsafe::AddFailure(TAG, "SSID too long");
        return;
    }

    if (password.length() > Constants::PasswordLength) {
        Failsafe::AddFailure(TAG, "Password too long");
        return;
    }

    if (address.length() > Constants::EndpointLength) {
        Failsafe::AddFailure(TAG, "Address too long");
        return;
    }

    if (AuthKey.length() > Constants::UUIDLength) {
        Failsafe::AddFailure(TAG, "Auth Key too long");
        return;
    }

    if (deviceName.length() > Constants::UUIDLength) {
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

    for (int i = 0; i < (Configuration::Sensor::SensorCount - 1); i++) {
        ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1,
                 storageData.Sensors[i] ? "enabled" : "disabled");
    }

    ESP_LOGI(TAG, "Saving data");
    ESP_ERROR_CHECK(
        nvs_set_blob(nvsHandle, TAG, &storageData, sizeof(StorageData)));
    ESP_ERROR_CHECK(nvs_commit(nvsHandle));
}

/**
 * @brief Resets the configuration data to the default values.
 *
 * This function performs the following steps:
 * 1. Logs the reset process.
 *
 * 2. Sets the configuration mode to true.
 *
 * 3. Resets the configuration data to the default values.
 *
 * 4. Saves the data to the NVS flash.
 */
void Reset() {
    ESP_LOGI(TAG, "Resetting data");

    storageData = {0};
    storageData.ConfigMode = true;

    ESP_ERROR_CHECK(
        nvs_set_blob(nvsHandle, TAG, &storageData, sizeof(StorageData)));

    ESP_ERROR_CHECK(nvs_commit(nvsHandle));
}

/**
 * @brief Mounts the SPIFFS partition.
 *
 * This function mounts the SPIFFS partition with the specified base path and
 * partition label. It performs the following steps:
 * 1. Logs the mounting process.
 *
 * 2. Registers the SPIFFS partition with the specified base path and partition
 * label.
 *
 * 3. Checks the partition information and formats it if the check fails.
 *
 * 4. Logs the partition information.
 *
 * @param base_path The base path for the mounted partition.
 * @param partition_label The label of the partition to mount.
 */
void Mount(const char* base_path, const char* partition_label) {
    ESP_LOGI(TAG, "Mounting partition %s - base path: %s", base_path,
             partition_label);

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
    esp_err_t err =
        esp_spiffs_info(storage_config.partition_label, &total, &used);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Getting partition information failed: %s - formatting",
                 esp_err_to_name(err));

        err = esp_spiffs_format(storage_config.partition_label);
        ESP_ERROR_CHECK(err);
    }

    ESP_LOGI(TAG, "Partition info: total: %u - used: %u", total, used);
}

/**
 * @brief Unmounts the SPIFFS partition.
 *
 * This function unmounts the SPIFFS partition with the specified partition
 * label. It performs the following steps:
 * 1. Logs the unmounting process.
 *
 * 2. Unregisters the SPIFFS partition with the specified partition label.
 *
 * @param partition_label The label of the partition to unmount.
 */
void CalculateMask() {
    ESP_LOGI(TAG, "Calculating encryption mask");

    uint32_t maskArray[10] = {0};
    uint64_t macArray[10] = {0};
    uint32_t mask = 1564230594;
    uint64_t mask2 = 0;

    union {
        uint64_t NumberRepresentation;
        uint8_t ArrayRepresentation[6];
    } mac = {0};

    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac.ArrayRepresentation));
    mask2 = mac.NumberRepresentation;

    for (int i = 10 - 1; i >= 0; i--) {
        macArray[i] = mask2 % 10;
        mask2 /= 10;
    }

    for (int i = 10 - 1; i >= 0; i--) {
        maskArray[i] = mask % 10;
        mask /= 10;
    }

    for (int i = 0; i <= 10 - 1; i += 2) {
        encryptionMask *= 10;
        encryptionMask += macArray[i];
    }

    for (int i = 9; i >= 1; i -= 2) {
        encryptionMask *= 10;
        encryptionMask += maskArray[i];
    }
}

/**
 * @brief Encrypts a given string and stores the result in the provided array.
 *
 * This function takes a string and encrypts each character using a predefined
 * encryption mask. The encrypted values are stored in the provided array of
 * `uint32_t`. The end of the encrypted data is marked by a zero.
 *
 * @param var Pointer to the array where the encrypted values will be stored.
 *            The array must be large enough to hold the encrypted values plus
 *            a terminating zero.
 * @param str The string to be encrypted.
 */
void EncryptText(uint32_t* var, const std::string& str) {
    for (const char c : str) {
        *(var++) = c ^ (uint32_t)encryptionMask;
    }

    *var = 0;
}

/**
 * @brief Decrypts the encrypted values stored in the provided array.
 *
 * This function takes an array of encrypted values and decrypts each value
 * using a predefined encryption mask. The decrypted values are concatenated
 * to form a string.
 *
 * @param var Pointer to the array of encrypted values.
 * @param str Reference to the string where the decrypted values will be stored.
 */
void DecryptText(uint32_t* var, std::string& str) {
    str.clear();

    while (*var) {
        char decryptedChar = char((*var ^ encryptionMask) & 0xFF);
        str.push_back(decryptedChar);
        ++var;
    }
}

/**
 * @brief Gets the SSID from the stored configuration data.
 *
 * @return The SSID from the stored configuration data.
 */
const std::string& GetSSID() {
    return ssid;
}

/**
 * @brief Gets the password from the stored configuration data.
 *
 * @return The password from the stored configuration data.
 */
const std::string& GetPassword() {
    return password;
}

/**
 * @brief Gets the address from the stored configuration data.
 *
 * @return The address from the stored configuration data.
 */
const std::string& GetAddress() {
    return address;
}

/**
 * @brief Gets the authentication key from the stored configuration data.
 *
 * @return The authentication key from the stored configuration data.
 */
const std::string& GetAuthKey() {
    return AuthKey;
}

/**
 * @brief Gets the device name from the stored configuration data.
 *
 * @return The device name from the stored configuration data.
 */
const std::string& GetDeviceName() {
    return deviceName;
}

/**
 * @brief Gets the device ID from the stored configuration data.
 *
 * @return The device ID from the stored configuration data.
 */
uint32_t GetDeviceId() {
    return storageData.DeviceId;
}

/**
 * @brief Gets the loudness threshold from the stored configuration data.
 *
 * @return The loudness threshold from the stored configuration data.
 */
uint32_t GetLoudnessThreshold() {
    return storageData.LoudnessThreshold;
}

/**
 * @brief Gets the register interval from the stored configuration data.
 *
 * @return The register interval from the stored configuration data.
 */
uint32_t GetRegisterInterval() {
    return storageData.RegisterInterval;
}

/**
 * @brief Gets the state of the specified sensor from the stored configuration
 * data.
 *
 * @param sensor The sensor for which to get the state.
 *
 * @return The state of the specified sensor from the stored configuration data.
 */
bool GetSensorState(Configuration::Sensor::Sensors sensor) {
    return storageData.Sensors[sensor - 1];
}

/**
 * @brief Gets the configuration mode from the stored configuration data.
 *
 * @return The configuration mode from the stored configuration data.
 */
bool GetConfigMode() {
    return storageData.ConfigMode;
};

/**
 * @brief Sets the SSID in the stored configuration data.
 *
 * @param str The SSID to set in the stored configuration data.
 */
void SetSSID(std::string&& str) {
    ssid = std::move(str);
}

/**
 * @brief Sets the password in the stored configuration data.
 *
 * @param str The password to set in the stored configuration data.
 */
void SetPassword(std::string&& str) {
    password = std::move(str);
}

/**
 * @brief Sets the address in the stored configuration data.
 *
 * @param str The address to set in the stored configuration data.
 */
void SetAddress(std::string&& str) {
    address = std::move(str);
}

/**
 * @brief Sets the authentication key in the stored configuration data.
 *
 * @param str The authentication key to set in the stored configuration data.
 */
void SetAuthKey(std::string&& str) {
    AuthKey = std::move(str);
}

/**
 * @brief Sets the device name in the stored configuration data.
 *
 * @param str The device name to set in the stored configuration data.
 */
void SetDeviceName(std::string&& str) {
    deviceName = std::move(str);
}

/**
 * @brief Sets the device ID in the stored configuration data.
 *
 * @param num The device ID to set in the stored configuration data.
 */
void SetDeviceId(uint32_t num) {
    storageData.DeviceId = num;
}

/**
 * @brief Sets the loudness threshold in the stored configuration data.
 *
 * @param num The loudness threshold to set in the stored configuration data.
 */
void SetLoudnessThreshold(uint32_t num) {
    storageData.LoudnessThreshold = num;
}

/**
 * @brief Sets the register interval in the stored configuration data.
 *
 * @param num The register interval to set in the stored configuration data.
 */
void SetRegisterInterval(uint32_t num) {
    storageData.RegisterInterval = num;
}

/**
 * @brief Sets the state of the specified sensor in the stored configuration
 * data.
 *
 * @param sensor The sensor for which to set the state.
 * @param state The state to set for the specified sensor.
 */
void SetSensorState(Configuration::Sensor::Sensors sensor, bool state) {
    storageData.Sensors[sensor - 1] = state;
}

/**
 * @brief Sets the configuration mode in the stored configuration data.
 *
 * @param config The configuration mode to set in the stored configuration data.
 */
void SetConfigMode(bool config) {
    storageData.ConfigMode = config;
}

}  // namespace Storage
