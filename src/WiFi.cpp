#include "WiFi.h"

#include <algorithm>
#include <cstring>

#include "Backend.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "HTTP.h"
#include "Network.h"
#include "Output.h"
#include "Storage.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_wifi_ap_get_sta_list.h"

namespace WiFi {

static const char *TAG = "WiFi";

static esp_netif_t *sta_netif = nullptr, *ap_netif = nullptr;
static EventGroupHandle_t wifi_event_group = nullptr;
static wifi_mode_t wifi_mode;
static EventBits_t event_bits;

static int retryAttempts = 0;
static bool passwordFailsafe = false;
static std::string ipAddress, macAddress;
static std::vector<ClientDetails> clientDetails(Constants::MaxClients);

/**
 * @brief WiFi event handler function.
 *
 * This function handles various WiFi events such as station connection,
 * disconnection, and IP acquisition. It performs actions based on the
 * type of event received.
 *
 * @param arg User-defined argument.
 * @param event_base Base ID of the event.
 * @param event_id ID of the event.
 * @param event_data Data associated with the event.
 */
static void wifi_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data
) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *event =
                (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(
                TAG, "Station " MACSTR " connected - aid: %d",
                MAC2STR(event->mac), event->aid
            );
        }

        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *event =
                (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(
                TAG, "Station " MACSTR " disconnected - aid: %d",
                MAC2STR(event->mac), event->aid
            );
        }

        if (event_id == WIFI_EVENT_STA_START) {
            ESP_ERROR_CHECK(esp_wifi_connect());
            Output::Blink(Output::LedY, 250, true);
        }

        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(wifi_event_group, States::Connected);
            ipAddress = "0.0.0.0";

            wifi_event_sta_disconnected_t *status =
                (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(
                TAG, "Disconnected from wifi - reason: %d", status->reason
            );

            if (status->reason == WIFI_REASON_ASSOC_LEAVE) {
                return;
            }

            if (!passwordFailsafe &&
                (status->reason == WIFI_REASON_AUTH_FAIL ||
                 status->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                 status->reason == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY
                )) {
                passwordFailsafe = true;
                Failsafe::AddFailure(
                    TAG, "Password: " + Storage::GetPassword() + " for SSID: " +
                             Storage::GetSSID() + " is not correct."
                );
            }

            if (retryAttempts < Constants::MaxRetries && !passwordFailsafe) {
                ESP_LOGI(TAG, "retrying to connect to AP");
                ESP_ERROR_CHECK(esp_wifi_connect());
                retryAttempts++;
            }

            else {
                retryAttempts = 0;
                Output::SetContinuity(Output::LedY, false);

                if (status->reason == WIFI_REASON_NO_AP_FOUND ||
                    status->reason ==
                        WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD) {
                    Failsafe::AddFailure(
                        TAG, "Can't find SSID: " + Storage::GetSSID()
                    );
                }

                if (Storage::GetConfigMode()) {
                    passwordFailsafe = false;
                    Network::Reset();
                }
            }
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t event = *(ip_event_got_ip_t *)event_data;

        char buffer[4 * 4 + 1] = {0};
        snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&event.ip_info.ip));

        ESP_LOGI(
            TAG, "Connected to WiFi - SSID: %s - Password: %s - IP: %s",
            Storage::GetSSID().c_str(), Storage::GetPassword().c_str(), buffer
        );

        ipAddress = buffer;
        retryAttempts = 0;

        xEventGroupSetBits(wifi_event_group, States::Connected);
        Output::SetContinuity(Output::LedY, false);
    }
}

/**
 * @brief Initializes the WiFi module.
 *
 * This function initializes the WiFi module by setting the MAC address,
 * initializing the network interface, creating the event loop, and initializing
 * the WiFi configuration. It registers the event handlers for WiFi and IP
 * events and creates the default station network interface. If the
 * configuration mode is enabled, it also creates the default access point
 * network interface.
 */
void Init() {
    SetMacAddress();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr
    ));

    sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, Configuration::WiFi::SSID)
    );

    if (Storage::GetConfigMode()) {
        ap_netif = esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(
            esp_netif_set_hostname(ap_netif, Configuration::WiFi::SSID)
        );
    }
}

/**
 * @brief Starts the WiFi module as an access point.
 *
 * This function starts the WiFi module as an access point by setting the
 * configuration parameters for the access point. It checks the length of the
 * SSID and password and sets the authentication mode accordingly. If the WiFi
 * mode is set to station, it disconnects the WiFi and stops the WiFi module
 * before starting the access point. It then sets the WiFi mode to access point,
 * sets the configuration, and starts the WiFi module. Finally, it starts the
 * HTTP server.
 */
void StartAP() {
    ESP_LOGI(TAG, "Starting as access point");

    wifi_config_t wifi_config = {
        .ap =
            {
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 4,
            },
    };

    size_t ssidLength = strlen(Configuration::WiFi::SSID);
    size_t passLength = strlen(Configuration::WiFi::Password);

    if (ssidLength <= 32) {
        memcpy(wifi_config.ap.ssid, Configuration::WiFi::SSID, ssidLength);
    }

    else {
        Failsafe::AddFailure(TAG, "SSID longer than 32 characters");
        strcpy((char *)wifi_config.ap.ssid, "Unit");
    }

    if (passLength >= 8 && passLength <= 64) {
        memcpy(
            wifi_config.ap.password, Configuration::WiFi::Password, passLength
        );
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    }

    else if (passLength) {
        Failsafe::AddFailure(TAG, "Password too long or too short");
    }

    if (wifi_mode == WIFI_MODE_STA) {
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

/**
 * @brief Starts the WiFi module as a station.
 *
 * This function starts the WiFi module as a station by setting the
 * configuration parameters for the station. It checks the length of the SSID
 * and password and sets the authentication mode accordingly. If the WiFi mode
 * is set to access point, it disconnects the WiFi and stops the WiFi module
 * before starting the station. It then sets the WiFi mode to station, sets the
 * configuration, and starts the WiFi module.
 */
void StartStation() {
    ESP_LOGI(TAG, "Starting as station");

    wifi_config_t wifi_config = {
        .sta =
            {
                .scan_method = WIFI_FAST_SCAN,
            },
    };

    if (Storage::GetSSID().length() <= 32) {
        memcpy(
            wifi_config.sta.ssid, Storage::GetSSID().c_str(),
            Storage::GetSSID().length()
        );
    }

    else {
        Failsafe::AddFailure(TAG, "SSID longer than 32 characters");
        return;
    }

    if (Storage::GetPassword().length() >= 8 &&
        Storage::GetPassword().length() <= 64) {
        memcpy(
            wifi_config.sta.password, Storage::GetPassword().c_str(),
            Storage::GetPassword().length()
        );
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    else if (Storage::GetPassword().length()) {
        Failsafe::AddFailure(TAG, "Password too long or too short");
        return;
    }

    if (wifi_mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Wifi mode AP, switching to Station");
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

    wifi_mode = WIFI_MODE_STA;
    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief Stops the WiFi module.
 *
 * This function stops the WiFi module by disconnecting the WiFi, stopping the
 * WiFi module, and clearing the event group bits. It also stops the HTTP server
 * if it is running.
 *
 * @return `true` if the WiFi module was stopped successfully, `false`
 * otherwise.
 */
bool IsConnected() {
    event_bits = xEventGroupWaitBits(
        wifi_event_group, States::Connected, pdFALSE, pdFALSE, 0
    );

    return event_bits & States::Connected;
}

/**
 * @brief Waits for the WiFi connection.
 *
 * This function waits for the WiFi connection by waiting for the event group
 * bits to be set to connected.
 */
void WaitForConnection() {
    xEventGroupWaitBits(
        wifi_event_group, States::Connected, pdFALSE, pdFALSE, portMAX_DELAY
    );
}

/**
 * @brief Sets the MAC address.
 *
 * This function reads the MAC address from the ESP32 chip and sets it as a
 * string.
 */
void SetMacAddress() {
    if (macAddress.empty()) {
        ESP_LOGI(TAG, "Reading mac");

        union {
            uint64_t NumberRepresentation;
            uint8_t ArrayRepresentation[6];
        } mac = {0};

        ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac.ArrayRepresentation));

        char buffer[Constants::MacLength] = {0};
        snprintf(
            buffer, sizeof(buffer), MACSTR, MAC2STR(mac.ArrayRepresentation)
        );
        macAddress = buffer;

        ESP_LOGI(TAG, "Mac set: %s", macAddress.c_str());
    }
}

/**
 * @brief Gets the IP address of the access point.
 *
 * This function gets the IP address of the access point and sets it as a
 * string.
 *
 * @return The IP address of the access point.
 */
const std::string &GetIPAP() {
    esp_netif_ip_info_t ip = {0};
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip));

    char buffer[4 * 4 + 1] = {0};
    snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip.ip));

    ipAddress = buffer;
    return ipAddress;
}

const std::vector<ClientDetails> &GetClientDetails() {
    wifi_sta_list_t wifi_sta_list = {0};
    wifi_sta_mac_ip_list_t wifi_sta_ip_mac_list = {0};

    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
    ESP_ERROR_CHECK(
        esp_wifi_ap_get_sta_list_with_ip(&wifi_sta_list, &wifi_sta_ip_mac_list)
    );

    clientDetails.clear();
    clientDetails.reserve(wifi_sta_ip_mac_list.num);

    for (int i = 0; i < wifi_sta_ip_mac_list.num; i++) {
        ClientDetails client = {0};
        const auto &station = wifi_sta_ip_mac_list.sta[i];
        snprintf(
            client.IPAddress, sizeof(client.IPAddress), IPSTR,
            IP2STR(&station.ip)
        );
        snprintf(
            client.MacAddress, sizeof(client.MacAddress), MACSTR,
            MAC2STR(station.mac)
        );
        clientDetails.push_back(client);
    }

    return clientDetails;
}

/**
 * @brief Gets the IP address of the station.
 *
 * This function gets the IP address of the station and sets it as a string.
 *
 * @return The IP address of the station.
 */
const std::string &GetIPStation() { return ipAddress; }

/**
 * @brief Gets the MAC address of the station.
 *
 * This function gets the MAC address of the station and sets it as a string.
 *
 * @return The MAC address of the station.
 */
const std::string &GetMacAddress() { return macAddress; };

}  // namespace WiFi