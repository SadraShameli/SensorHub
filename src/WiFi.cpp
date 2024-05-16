#include <cstring>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"
#include "Backend.h"
#include "Storage.h"
#include "Failsafe.h"
#include "Output.h"
#include "WiFi.h"
#include "HTTP.h"
#include "Network.h"

static const char *TAG = "Wifi";
static esp_netif_t *sta_netif, *ap_netif;
static wifi_mode_t wifi_mode;
static EventBits_t event_bits;
static EventGroupHandle_t s_wifi_event_group;

static int s_RetriesAttempts;
static std::string s_IPAddress = "0.0.0.0";

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " connected, AID=%d", MAC2STR(event->mac), event->aid);
        }

        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station " MACSTR " disconnected, AID=%d", MAC2STR(event->mac), event->aid);
        }

        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();

            Output::Blink(DeviceConfig::Outputs::LedY, 250, true);
        }

        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            ESP_LOGI(TAG, "Disconnected from wifi");
            xEventGroupClearBits(s_wifi_event_group, WiFi::State::Connected);
            s_IPAddress = "0.0.0.0";

            wifi_event_sta_disconnected_t *status = (wifi_event_sta_disconnected_t *)event_data;
            if (status->reason == WIFI_REASON_ASSOC_LEAVE)
            {
                return;
            }

            if (status->reason == WIFI_REASON_NO_AP_FOUND || status->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || status->reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
            {
                Failsafe::AddFailure("Failed to connect to SSID: " + Backend::SSID + " - Password: " + Backend::Password);
            }

            if (s_RetriesAttempts < DeviceConfig::WiFi::ConnectionRetries)
            {
                esp_wifi_connect();
                s_RetriesAttempts++;
                ESP_LOGI(TAG, "retrying to connect to AP");
            }

            else
            {
                Failsafe::AddFailure("Can't connect to the WiFi");
                xEventGroupSetBits(s_wifi_event_group, WiFi::State::Failed);
            }
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t event = *(ip_event_got_ip_t *)event_data;

        char buffer[4 * 4 + 1] = {};
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&event.ip_info.ip));

        ESP_LOGI(TAG, "Connected to WiFi - SSID: %s - Password: %s - IP: %s", Backend::SSID.c_str(), Backend::Password.c_str(), buffer);

        s_IPAddress = buffer;
        s_RetriesAttempts = 0;
        xEventGroupSetBits(s_wifi_event_group, WiFi::State::Connected);
        xEventGroupClearBits(s_wifi_event_group, WiFi::State::Failed);
        Output::SetContinuity(DeviceConfig::Outputs::LedY, false);
    }
}

void WiFi::Init()
{
    SetMacAddress();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_wifi_event_group = xEventGroupCreate();
    configASSERT(s_wifi_event_group);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    sta_netif = esp_netif_create_default_wifi_sta();
    esp_err_t err = esp_netif_set_hostname(sta_netif, DeviceConfig::WiFi::SSID);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Setting wifi hostname failed");
    }

    if (Storage::GetConfigMode())
    {
        ap_netif = esp_netif_create_default_wifi_ap();
        err = esp_netif_set_hostname(ap_netif, DeviceConfig::WiFi::SSID);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Setting wifi hostname failed");
        }
    }
}

void WiFi::StartAP()
{
    ESP_LOGI(TAG, "Starting Wifi as access point");

    wifi_config_t wifi_config = {
        .ap = {
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };

    size_t ssidLength = strlen(DeviceConfig::WiFi::SSID);
    size_t passLength = strlen(DeviceConfig::WiFi::PASS);

    if (ssidLength <= 32)
    {
        memcpy(wifi_config.ap.ssid, DeviceConfig::WiFi::SSID, ssidLength);
    }

    else
    {
        ESP_LOGW(TAG, "SSID longer than 32 characters");
        strcpy((char *)wifi_config.ap.ssid, "Blue Star Planning");
    }

    if (passLength >= 8 && passLength <= 64)
    {
        memcpy(wifi_config.ap.password, DeviceConfig::WiFi::PASS, passLength);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    }

    else if (passLength != 0)
    {
        ESP_LOGW(TAG, "Password too long or too short");
    }

    if (wifi_mode == WIFI_MODE_STA)
    {
        ESP_LOGI(TAG, "Wifi mode Station, switching to AP");

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

    wifi_mode = WIFI_MODE_AP;

    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    HTTP::StartServer();
}

void WiFi::StartStation()
{
    ESP_LOGI(TAG, "Starting Wifi as station");

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .threshold = {
                .rssi = -127,
            },
            .pmf_cfg = {
                .capable = true,
            },
        },
    };

    if (Backend::SSID.length() <= 32)
    {
        memcpy(wifi_config.sta.ssid, Backend::SSID.c_str(), Backend::SSID.length());
    }

    else
    {
        ESP_LOGW(TAG, "SSID longer than 32 characters");

        return;
    }

    if (Backend::Password.length() >= 8 && Backend::Password.length() <= 64)
    {
        memcpy(wifi_config.sta.password, Backend::Password.c_str(), Backend::Password.length());
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    else
    {
        ESP_LOGW(TAG, "Password too long or too short");
    }

    if (wifi_mode == WIFI_MODE_AP)
    {
        ESP_LOGI(TAG, "Wifi mode AP, switching to Station");

        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

    wifi_mode = WIFI_MODE_STA;

    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool WiFi::IsConnected()
{
    event_bits = xEventGroupWaitBits(s_wifi_event_group, WiFi::State::Connected, pdFALSE, pdFALSE, pdMS_TO_TICKS(25));
    return event_bits & WiFi::State::Connected;
}

void WiFi::SetMacAddress()
{
    if (m_MacAddress.empty())
    {
        ESP_LOGI(TAG, "Reading Mac Address");

        union
        {
            uint64_t NumberRepresentation;
            uint8_t ArrayRepresentation[6];
        } mac = {};

        if (esp_efuse_mac_get_default(mac.ArrayRepresentation) != ESP_OK)
        {
            ESP_LOGE(TAG, "Reading mac from efuse failed");
            return;
        }

        char buffer[MAC_LEN] = {};
        snprintf(buffer, sizeof(buffer), MACSTR, MAC2STR(mac.ArrayRepresentation));
        m_MacAddress = buffer;

        ESP_LOGI(TAG, "Mac Address set: %s", buffer);
    }
}

const std::string &WiFi::GetIPAP()
{
    esp_netif_ip_info_t ip = {};
    esp_netif_get_ip_info(ap_netif, &ip);

    char buffer[4 * 4 + 1] = {};
    snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip.ip));

    s_IPAddress = buffer;
    return s_IPAddress;
}

const std::string &WiFi::GetIPStation() { return s_IPAddress; }

const std::vector<WiFi::ClientDetails> WiFi::GetClientDetails()
{
    wifi_sta_list_t wifi_sta_list = {};
    wifi_sta_mac_ip_list_t wifi_sta_ip_mac_list = {};

    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_ap_get_sta_list_with_ip(&wifi_sta_list, &wifi_sta_ip_mac_list);

    std::vector<ClientDetails> clientDetails(wifi_sta_ip_mac_list.num);

    for (int i = 0; i < wifi_sta_ip_mac_list.num; i++)
    {
        auto &station = wifi_sta_ip_mac_list.sta[i];
        snprintf(clientDetails[i].IPAddress, sizeof(clientDetails[i].IPAddress), IPSTR, IP2STR(&station.ip));
        snprintf(clientDetails[i].MacAddress, sizeof(clientDetails[i].MacAddress), MACSTR, MAC2STR(station.mac));
    }

    return clientDetails;
}