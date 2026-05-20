#include "Storage.h"

#include <cstring>

#include "Configuration.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace Storage {

namespace Keys {

static constexpr const char* kNamespace = "cfg";
static constexpr const char* kSchemaVer = "schema_ver";
static constexpr const char* kSsid = "wifi_ssid";
static constexpr const char* kPass = "wifi_pass";
static constexpr const char* kSrvUrl = "srv_url";
static constexpr const char* kAuthKey = "auth_key";
static constexpr const char* kDevName = "dev_name";
static constexpr const char* kApPass = "ap_pass";
static constexpr const char* kDevId = "dev_id";
static constexpr const char* kLoudThresh = "loud_thresh";
static constexpr const char* kRegInterval = "reg_interval";
static constexpr const char* kSensorsMask = "sensors_mask";
static constexpr const char* kCfgMode = "cfg_mode";

}

namespace Limits {

static constexpr size_t kSsidMax = 33;
static constexpr size_t kPassMax = 65;
static constexpr size_t kUrlMax = 241;
static constexpr size_t kUuidMax = 37;
static constexpr size_t kDevTokenMax = 64;
static constexpr size_t kApPassMax = 17;

}

static constexpr uint32_t kSchemaVersion = 2;
static const char* TAG = "Storage";

static struct {
    std::string ssid;
    std::string password;
    std::string address;
    std::string authKey;
    std::string deviceName;
    std::string apPassword;
    uint32_t deviceId = 0;
    uint32_t loudnessThreshold = 0;
    uint32_t registerInterval = 0;
    uint32_t sensorsMask = 0;
    bool configMode = true;
} g_cache;

static nvs_handle_t g_nvs = 0;

namespace {

esp_err_t ReadStr(const char* key, std::string& out, size_t max_len) {
    size_t len = 0;
    esp_err_t err = nvs_get_str(g_nvs, key, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out.clear();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (len > max_len) {
        ESP_LOGW(TAG,
                 "Stored '%s' too long (%u > %u), discarding",
                 key,
                 (unsigned)len,
                 (unsigned)max_len);
        out.clear();
        return ESP_OK;
    }
    out.resize(len);
    err = nvs_get_str(g_nvs, key, out.data(), &len);
    if (err == ESP_OK && !out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return err;
}

esp_err_t ReadU32(const char* key, uint32_t& out, uint32_t fallback = 0) {
    esp_err_t err = nvs_get_u32(g_nvs, key, &out);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out = fallback;
        return ESP_OK;
    }
    return err;
}

esp_err_t ReadU8(const char* key, uint8_t& out, uint8_t fallback = 0) {
    esp_err_t err = nvs_get_u8(g_nvs, key, &out);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out = fallback;
        return ESP_OK;
    }
    return err;
}

void LoadAll() {
    ESP_ERROR_CHECK(ReadStr(Keys::kSsid, g_cache.ssid, Limits::kSsidMax));
    ESP_ERROR_CHECK(ReadStr(Keys::kPass, g_cache.password, Limits::kPassMax));
    ESP_ERROR_CHECK(ReadStr(Keys::kSrvUrl, g_cache.address, Limits::kUrlMax));
    ESP_ERROR_CHECK(ReadStr(Keys::kAuthKey, g_cache.authKey, Limits::kDevTokenMax));
    ESP_ERROR_CHECK(
        ReadStr(Keys::kDevName, g_cache.deviceName, Limits::kUuidMax));
    ESP_ERROR_CHECK(
        ReadStr(Keys::kApPass, g_cache.apPassword, Limits::kApPassMax));

    ESP_ERROR_CHECK(ReadU32(Keys::kDevId, g_cache.deviceId));
    ESP_ERROR_CHECK(ReadU32(Keys::kLoudThresh, g_cache.loudnessThreshold));
    ESP_ERROR_CHECK(ReadU32(Keys::kRegInterval, g_cache.registerInterval));
    ESP_ERROR_CHECK(ReadU32(Keys::kSensorsMask, g_cache.sensorsMask));

    uint8_t cfg = 1;
    ESP_ERROR_CHECK(ReadU8(Keys::kCfgMode, cfg, 1));
    g_cache.configMode = (cfg != 0);
}

bool StoredStrDiffers(const char* key, const std::string& value) {
    size_t stored_len = 0;
    esp_err_t err = nvs_get_str(g_nvs, key, nullptr, &stored_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return !value.empty();
    }
    if (err != ESP_OK) {
        return true;
    }
    if (stored_len == 0) {
        return !value.empty();
    }
    if (stored_len - 1 != value.size()) {
        return true;
    }
    std::string stored;
    stored.resize(stored_len);
    if (nvs_get_str(g_nvs, key, stored.data(), &stored_len) != ESP_OK) {
        return true;
    }
    if (!stored.empty() && stored.back() == '\0') {
        stored.pop_back();
    }
    return stored != value;
}

template <typename Getter>
bool StoredScalarDiffers(const char* key, Getter getter, auto value) {
    decltype(value) stored{};
    esp_err_t err = getter(g_nvs, key, &stored);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return value != decltype(value){};
    }
    if (err != ESP_OK) {
        return true;
    }
    return stored != value;
}

void WriteStrIfChanged(const char* key, const std::string& value) {
    if (StoredStrDiffers(key, value)) {
        ESP_ERROR_CHECK(nvs_set_str(g_nvs, key, value.c_str()));
    }
}

void WriteU32IfChanged(const char* key, uint32_t value) {
    if (StoredScalarDiffers(key, nvs_get_u32, value)) {
        ESP_ERROR_CHECK(nvs_set_u32(g_nvs, key, value));
    }
}

void WriteU8IfChanged(const char* key, uint8_t value) {
    if (StoredScalarDiffers(key, nvs_get_u8, value)) {
        ESP_ERROR_CHECK(nvs_set_u8(g_nvs, key, value));
    }
}

void WriteAll() {
    WriteStrIfChanged(Keys::kSsid, g_cache.ssid);
    WriteStrIfChanged(Keys::kPass, g_cache.password);
    WriteStrIfChanged(Keys::kSrvUrl, g_cache.address);
    WriteStrIfChanged(Keys::kAuthKey, g_cache.authKey);
    WriteStrIfChanged(Keys::kDevName, g_cache.deviceName);
    WriteStrIfChanged(Keys::kApPass, g_cache.apPassword);

    WriteU32IfChanged(Keys::kDevId, g_cache.deviceId);
    WriteU32IfChanged(Keys::kLoudThresh, g_cache.loudnessThreshold);
    WriteU32IfChanged(Keys::kRegInterval, g_cache.registerInterval);
    WriteU32IfChanged(Keys::kSensorsMask, g_cache.sensorsMask);

    WriteU8IfChanged(Keys::kCfgMode,
                     static_cast<uint8_t>(g_cache.configMode ? 1 : 0));
}

uint32_t SensorBit(Configuration::Sensor::Sensors sensor) {

    return 1u << (static_cast<uint32_t>(sensor) - 1u);
}

}

void Init() {
    ESP_LOGI(TAG, "Initializing");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(nvs_open(Keys::kNamespace, NVS_READWRITE, &g_nvs));

    uint32_t ver = 0;
    err = nvs_get_u32(g_nvs, Keys::kSchemaVer, &ver);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG,
                 "Fresh NVS, writing schema v%lu defaults",
                 (unsigned long)kSchemaVersion);
        g_cache = {};
        g_cache.configMode = true;
        WriteAll();
        ESP_ERROR_CHECK(nvs_set_u32(g_nvs, Keys::kSchemaVer, kSchemaVersion));
        ESP_ERROR_CHECK(nvs_commit(g_nvs));
        return;
    }
    ESP_ERROR_CHECK(err);

    if (ver != kSchemaVersion) {
        ESP_LOGE(TAG,
                 "Schema version mismatch (stored %lu, expected %lu)",
                 (unsigned long)ver,
                 (unsigned long)kSchemaVersion);

        Reset();
        return;
    }

    LoadAll();

    ESP_LOGI(
        TAG,
        "Loaded: cfg_mode=%d ssid=%u chars url=%s dev_id=%lu sensors=0x%lx",
        g_cache.configMode ? 1 : 0,
        (unsigned)g_cache.ssid.length(),
        g_cache.address.c_str(),
        (unsigned long)g_cache.deviceId,
        (unsigned long)g_cache.sensorsMask);
}

void Commit() {
    if (g_cache.ssid.length() >= Limits::kSsidMax) {
        ESP_LOGE(TAG, "SSID too long");
        return;
    }
    if (g_cache.password.length() >= Limits::kPassMax) {
        ESP_LOGE(TAG, "Password too long");
        return;
    }
    if (g_cache.address.length() >= Limits::kUrlMax) {
        ESP_LOGE(TAG, "Address too long");
        return;
    }
    if (g_cache.authKey.length() >= Limits::kDevTokenMax) {
        ESP_LOGE(TAG, "Auth Key too long");
        return;
    }
    if (g_cache.deviceName.length() >= Limits::kUuidMax) {
        ESP_LOGE(TAG, "Device Name too long");
        return;
    }
    if (g_cache.apPassword.length() >= Limits::kApPassMax) {
        ESP_LOGE(TAG, "AP Password too long");
        return;
    }

    ESP_LOGI(TAG, "Committing config");
    WriteAll();
    ESP_ERROR_CHECK(nvs_set_u32(g_nvs, Keys::kSchemaVer, kSchemaVersion));
    ESP_ERROR_CHECK(nvs_commit(g_nvs));
}

void Reset() {
    ESP_LOGW(TAG, "Erasing all stored configuration");
    ESP_ERROR_CHECK(nvs_erase_all(g_nvs));
    g_cache = {};
    g_cache.configMode = true;
    WriteAll();
    ESP_ERROR_CHECK(nvs_set_u32(g_nvs, Keys::kSchemaVer, kSchemaVersion));
    ESP_ERROR_CHECK(nvs_commit(g_nvs));
}

void Mount(const char* base_path, const char* partition_label) {
    ESP_LOGI(TAG, "Mounting partition %s at %s", partition_label, base_path);

    esp_vfs_spiffs_conf_t storage_config = {
        .base_path = base_path,
        .partition_label = partition_label,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&storage_config));

#ifdef UNIT_DEBUG
    ESP_ERROR_CHECK(esp_spiffs_check(partition_label));
#endif

    size_t total = 0, used = 0;
    esp_err_t err =
        esp_spiffs_info(storage_config.partition_label, &total, &used);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Partition info failed: %s; formatting",
                 esp_err_to_name(err));
        ESP_ERROR_CHECK(esp_spiffs_format(storage_config.partition_label));
        return;
    }

    ESP_LOGI(TAG, "Partition info: total=%u used=%u", total, used);
}

const std::string& GetSSID() {
    return g_cache.ssid;
}

const std::string& GetPassword() {
    return g_cache.password;
}

const std::string& GetAddress() {
    return g_cache.address;
}

const std::string& GetAuthKey() {
    return g_cache.authKey;
}

const std::string& GetDeviceName() {
    return g_cache.deviceName;
}

const std::string& GetApPassword() {
    return g_cache.apPassword;
}

uint32_t GetDeviceId() {
    return g_cache.deviceId;
}

uint32_t GetLoudnessThreshold() {
    return g_cache.loudnessThreshold;
}

uint32_t GetRegisterInterval() {
    return g_cache.registerInterval;
}

bool GetConfigMode() {
    return g_cache.configMode;
}

bool GetSensorState(Configuration::Sensor::Sensors sensor) {
    return (g_cache.sensorsMask & SensorBit(sensor)) != 0;
}

void SetSSID(std::string&& s) {
    g_cache.ssid = std::move(s);
}

void SetPassword(std::string&& s) {
    g_cache.password = std::move(s);
}

void SetAddress(std::string&& s) {
    g_cache.address = std::move(s);
}

void SetAuthKey(std::string&& s) {
    g_cache.authKey = std::move(s);
}

void SetDeviceName(std::string&& s) {
    g_cache.deviceName = std::move(s);
}

void SetApPassword(std::string&& s) {
    g_cache.apPassword = std::move(s);
}

void SetDeviceId(uint32_t v) {
    g_cache.deviceId = v;
}

void SetLoudnessThreshold(uint32_t v) {
    g_cache.loudnessThreshold = v;
}

void SetRegisterInterval(uint32_t v) {
    g_cache.registerInterval = v;
}

void SetConfigMode(bool v) {
    g_cache.configMode = v;
}

void SetSensorState(Configuration::Sensor::Sensors sensor, bool state) {
    const uint32_t bit = SensorBit(sensor);
    if (state) {
        g_cache.sensorsMask |= bit;
    } else {
        g_cache.sensorsMask &= ~bit;
    }
}

const Kernel::Service kService = {
    .name = "Storage",
    .modes = Kernel::RunAlways,
    .on_init = &Init,
    .task_entry = nullptr,
    .stack_bytes = 0,
    .priority = 0,
    .out_handle = nullptr,
    .should_start = nullptr,
};

}
