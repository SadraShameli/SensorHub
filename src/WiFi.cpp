#include <cstring>
#include <algorithm>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"
#include "Backend.h"
#include "Configuration.h"
#include "Storage.h"
#include "Failsafe.h"
#include "Output.h"
#include "HTTP.h"
#include "Network.h"
#include "WiFi.h"

namespace WiFi
{
    static const char *TAG = "WiFi";
    static esp_netif_t *sta_netif = nullptr, *ap_netif = nullptr;
    static EventGroupHandle_t wifi_event_group = nullptr;
    static wifi_mode_t wifi_mode;
    static EventBits_t event_bits;

    static int retryAttempts = 0;
    static bool passwordFailsafe = false;
    static std::string ipAddress, macAddress;
    static std::vector<ClientDetails> clientDetails(Constants::MaxClients);

    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base == WIFI_EVENT)
        {
            if (event_id == WIFI_EVENT_AP_STACONNECTED)
            {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " connected - aid: %d", MAC2STR(event->mac), event->aid);
            }

            else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
            {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " disconnected - aid: %d", MAC2STR(event->mac), event->aid);
            }

            if (event_id == WIFI_EVENT_STA_START)
            {
                ESP_ERROR_CHECK(esp_wifi_connect());
                Output::Blink(Output::LedY, 250, true);
            }

            else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
            {
                xEventGroupClearBits(wifi_event_group, States::Connected);
                ipAddress = "0.0.0.0";

                wifi_event_sta_disconnected_t *status = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "Disconnected from wifi - reason: %d", status->reason);

                if (status->reason == WIFI_REASON_ASSOC_LEAVE)
                    return;

                if (status->reason == WIFI_REASON_AUTH_FAIL && !passwordFailsafe)
                {
                    passwordFailsafe = true;
                    Failsafe::AddFailure(TAG, "Password: " + Storage::GetPassword() + " for SSID: " + Storage::GetSSID() + " is not correct.");
                }

                if (retryAttempts < Constants::MaxRetries && !passwordFailsafe)
                {
                    ESP_LOGI(TAG, "retrying to connect to AP");
                    ESP_ERROR_CHECK(esp_wifi_connect());
                    retryAttempts++;
                }

                else
                {
                    if (Storage::GetConfigMode())
                    {
                        passwordFailsafe = false;
                        Network::Reset();
                    }

                    Output::SetContinuity(Output::LedY, false);

                    if (status->reason == WIFI_REASON_NO_AP_FOUND || status->reason == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD)
                    {
                        Failsafe::AddFailure(TAG, "Can't find SSID: " + Storage::GetSSID());
                        return;
                    }
                }
            }
        }

        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t event = *(ip_event_got_ip_t *)event_data;

            char buffer[4 * 4 + 1] = {0};
            snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&event.ip_info.ip));

            ESP_LOGI(TAG, "Connected to WiFi - SSID: %s - Password: %s - IP: %s", Storage::GetSSID().c_str(), Storage::GetPassword().c_str(), buffer);

            ipAddress = buffer;
            retryAttempts = 0;

            xEventGroupSetBits(wifi_event_group, States::Connected);
            Output::SetContinuity(Output::LedY, false);
        }
    }

    void Init()
    {
        SetMacAddress();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        wifi_event_group = xEventGroupCreate();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

        sta_netif = esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, Configuration::WiFi::SSID));

        if (Storage::GetConfigMode())
        {
            ap_netif = esp_netif_create_default_wifi_ap();
            ESP_ERROR_CHECK(esp_netif_set_hostname(ap_netif, Configuration::WiFi::SSID));
        }
    }

    void StartAP()
    {
        ESP_LOGI(TAG, "Starting as access point");

        wifi_config_t wifi_config = {
            .ap = {
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 4,
            },
        };

        size_t ssidLength = strlen(Configuration::WiFi::SSID);
        size_t passLength = strlen(Configuration::WiFi::Password);

        if (ssidLength <= 32)
            memcpy(wifi_config.ap.ssid, Configuration::WiFi::SSID, ssidLength);

        else
        {
            Failsafe::AddFailure(TAG, "SSID longer than 32 characters");
            strcpy((char *)wifi_config.ap.ssid, "Unit");
        }

        if (passLength >= 8 && passLength <= 64)
        {
            memcpy(wifi_config.ap.password, Configuration::WiFi::Password, passLength);
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
        }

        else if (passLength != 0)
            Failsafe::AddFailure(TAG, "Password too long or too short");

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

    void StartStation()
    {
        ESP_LOGI(TAG, "Starting as station");

        wifi_config_t wifi_config = {
            .sta = {
                .scan_method = WIFI_FAST_SCAN,
            },
        };

        if (Storage::GetSSID().length() <= 32)
            memcpy(wifi_config.sta.ssid, Storage::GetSSID().c_str(), Storage::GetSSID().length());

        else
        {
            Failsafe::AddFailure(TAG, "SSID longer than 32 characters");
            return;
        }

        if (Storage::GetPassword().length() >= 8 && Storage::GetPassword().length() <= 64)
        {
            memcpy(wifi_config.sta.password, Storage::GetPassword().c_str(), Storage::GetPassword().length());
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        }

        else
        {
            Failsafe::AddFailure(TAG, "Password too long or too short");
            return;
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

    bool IsConnected()
    {
        event_bits = xEventGroupWaitBits(wifi_event_group, States::Connected, pdFALSE, pdFALSE, 0);
        return event_bits & States::Connected;
    }

    void WaitForConnection()
    {
        xEventGroupWaitBits(wifi_event_group, States::Connected, pdFALSE, pdFALSE, portMAX_DELAY);
    }

    void SetMacAddress()
    {
        if (macAddress.empty())
        {
            ESP_LOGI(TAG, "Reading mac");

            union
            {
                uint64_t NumberRepresentation;
                uint8_t ArrayRepresentation[6];
            } mac = {0};

            ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac.ArrayRepresentation));

            char buffer[Constants::MacLength] = {0};
            snprintf(buffer, sizeof(buffer), MACSTR, MAC2STR(mac.ArrayRepresentation));
            macAddress = buffer;

            ESP_LOGI(TAG, "Mac set: %s", macAddress.c_str());
        }
    }

    const std::string &GetIPAP()
    {
        esp_netif_ip_info_t ip = {0};
        ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip));

        char buffer[4 * 4 + 1] = {0};
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip.ip));

        ipAddress = buffer;
        return ipAddress;
    }

    const std::vector<ClientDetails> &GetClientDetails()
    {
        wifi_sta_list_t wifi_sta_list = {0};
        wifi_sta_mac_ip_list_t wifi_sta_ip_mac_list = {0};

        ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
        ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list_with_ip(&wifi_sta_list, &wifi_sta_ip_mac_list));

        clientDetails.clear();
        clientDetails.reserve(wifi_sta_ip_mac_list.num);

        for (int i = 0; i < wifi_sta_ip_mac_list.num; i++)
        {
            ClientDetails client = {0};
            const auto &station = wifi_sta_ip_mac_list.sta[i];
            snprintf(client.IPAddress, sizeof(client.IPAddress), IPSTR, IP2STR(&station.ip));
            snprintf(client.MacAddress, sizeof(client.MacAddress), MACSTR, MAC2STR(station.mac));
            clientDetails.push_back(client);
        }

        return clientDetails;
    }

    const std::string &GetIPStation() { return ipAddress; }
    const std::string &GetMacAddress() { return macAddress; };
}