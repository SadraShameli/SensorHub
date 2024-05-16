#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "Failsafe.h"
#include "Storage.h"

static const char *TAG = "Storage";
static nvs_handle_t nvsHandle;

void Storage::MountSPIFFS(const char *base_path, const char *partition_label)
{
    ESP_LOGI(TAG, "Mounting partition %s with base path: %s", base_path, partition_label);

    esp_vfs_spiffs_conf_t storage_config = {
        .base_path = base_path,
        .partition_label = partition_label,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&storage_config));

#ifdef UNIT_DEBUG
    ESP_LOGI(TAG, "Performing SPIFFS check");

    ESP_ERROR_CHECK(esp_spiffs_check(partition_label));

    ESP_LOGI(TAG, "SPIFFS Check successful");
#endif

    size_t total = 0, used = 0;
    esp_err_t err = esp_spiffs_info(storage_config.partition_label, &total, &used);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Getting partition information failed: %s - formatting", esp_err_to_name(err));
        esp_spiffs_format(storage_config.partition_label);
        ESP_ERROR_CHECK(err);
    }

    ESP_LOGI(TAG, "Partition info: Total: %u - used: %u", total, used);
}

void Storage::Init()
{
    size_t required_size = 0;
    m_StorageData.ConfigMode = true;

    CalculateMask();
    ESP_LOGI(TAG, "Initializing NVS storage");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "NVS initialization failed");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &nvsHandle));

    ESP_LOGI(TAG, "Getting NVS storage saved data");
    nvs_get_blob(nvsHandle, TAG, nullptr, &required_size);
    nvs_get_blob(nvsHandle, TAG, &m_StorageData, &required_size);

    ESP_LOGI(TAG, "Config Mode: %s", m_StorageData.ConfigMode == true ? "true" : "false");
    if (!m_StorageData.ConfigMode)
    {
        m_SSID.reserve(SSIDLength);
        DecryptText(m_StorageData.SSID, m_SSID);
        ESP_LOGI(TAG, "Decrypted SSID: %s", m_SSID.c_str());

        m_Password.reserve(PasswordLength);
        DecryptText(m_StorageData.Password, m_Password);
        ESP_LOGI(TAG, "Decrypted Password: %s", m_Password.c_str());

        m_Address.reserve(EndpointLength);
        DecryptText(m_StorageData.Address, m_Address);
        ESP_LOGI(TAG, "Decrypted Address: %s", m_Address.c_str());

        m_AuthKey.reserve(UUIDLength);
        DecryptText(m_StorageData.AuthKey, m_AuthKey);
        ESP_LOGI(TAG, "Decrypted Auth Key: %s", m_AuthKey.c_str());

        m_DeviceName.reserve(UUIDLength);
        DecryptText(m_StorageData.DeviceName, m_DeviceName);
        ESP_LOGI(TAG, "Decrypted Device Name: %s", m_DeviceName.c_str());

        ESP_LOGI(TAG, "Device Type: %ld", m_StorageData.DeviceType);
        ESP_LOGI(TAG, "Device Id: %ld", m_StorageData.DeviceId);
        ESP_LOGI(TAG, "Loudness Threshold: %ld", m_StorageData.LoudnessThreshold);
        ESP_LOGI(TAG, "Register Interval: %ld", m_StorageData.RegisterInterval);

        for (int i = 0; i < (Backend::SensorTypes::SensorCount - 1); i++)
        {
            ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1, m_StorageData.Sensors[i] ? "enabled" : "disabled");
        }
    }
}

void Storage::Commit()
{
    if (m_SSID.length() > SSIDLength)
    {
        Failsafe::AddFailure("SSID too long");
        return;
    }

    if (m_Password.length() > PasswordLength)
    {
        Failsafe::AddFailure("Password too long");
        return;
    }

    if (m_Address.length() > EndpointLength)
    {
        Failsafe::AddFailure("Address too long");
        return;
    }

    if (m_AuthKey.length() > UUIDLength)
    {
        Failsafe::AddFailure("Auth Key too long");
        return;
    }

    if (m_DeviceName.length() > UUIDLength)
    {
        Failsafe::AddFailure("Device Name too long");
        return;
    }

    ESP_LOGI(TAG, "Encrypting SSID: %s", m_SSID.c_str());
    EncryptText(m_StorageData.SSID, m_SSID);

    ESP_LOGI(TAG, "Encrypting Password: %s", m_Password.c_str());
    EncryptText(m_StorageData.Password, m_Password);

    ESP_LOGI(TAG, "Encrypting Address: %s", m_Address.c_str());
    EncryptText(m_StorageData.Address, m_Address);

    ESP_LOGI(TAG, "Encrypting Auth Key: %s", m_AuthKey.c_str());
    EncryptText(m_StorageData.AuthKey, m_AuthKey);

    ESP_LOGI(TAG, "Encrypting Device Name: %s", m_DeviceName.c_str());
    EncryptText(m_StorageData.DeviceName, m_DeviceName);

    ESP_LOGI(TAG, "Device Type: %ld", m_StorageData.DeviceType);
    ESP_LOGI(TAG, "Device Id: %ld", m_StorageData.DeviceId);
    ESP_LOGI(TAG, "Loudness Threshold: %ld", m_StorageData.LoudnessThreshold);
    ESP_LOGI(TAG, "Register Interval: %ld", m_StorageData.RegisterInterval);

    for (int i = 0; i < (Backend::SensorTypes::SensorCount - 1); i++)
    {
        ESP_LOGI(TAG, "Sensor %d - state: %s", i + 1, m_StorageData.Sensors[i] ? "enabled" : "disabled");
    }

    ESP_LOGI(TAG, "Saving to storage blob");
    ESP_ERROR_CHECK(nvs_set_blob(nvsHandle, TAG, &m_StorageData, sizeof(StorageData)));
    ESP_ERROR_CHECK(nvs_commit(nvsHandle));
}

void Storage::Reset()
{
    ESP_LOGI(TAG, "Resetting storage blob");

    m_StorageData = {};
    m_StorageData.ConfigMode = true;

    return Commit();
}

void Storage::CalculateMask()
{
    ESP_LOGI(TAG, "Calculating encryption mask");

    uint32_t maskArray[10] = {};
    uint32_t macArray[10] = {};
    uint32_t mask = 1564230594;
    uint64_t mask2 = 0;

    union
    {
        uint64_t NumberRepresentation;
        uint8_t ArrayRepresentation[6];
    } mac = {};

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
        m_EncryptionMask *= 10;
        m_EncryptionMask += macArray[i];
    }

    for (int i = 9; i >= 1; i -= 2)
    {
        m_EncryptionMask *= 10;
        m_EncryptionMask += maskArray[i];
    }
}

void Storage::EncryptText(uint32_t *var, const std::string &str)
{
    for (const char &c : str)
    {
        *(var++) = c ^ m_EncryptionMask;
    }
}

void Storage::DecryptText(uint32_t *var, std::string &str)
{
    str.clear();

    while (*var)
    {
        str.push_back(*(var++) ^ m_EncryptionMask);
    }
}
