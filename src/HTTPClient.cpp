#include "esp_http_client.h"
#include "esp_tls.h"
#include "ArduinoJson.h"
#include "Definitions.h"
#include "Backend.h"
#include "Failsafe.h"
#include "WiFi.h"
#include "HTTP.h"

static const char *TAG = "HTTPClient";
static esp_http_client_handle_t httpClient = nullptr;
static std::string httpBuffer;

static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ERROR || evt->event_id == HTTP_EVENT_HEADERS_SENT)
    {
        httpBuffer.clear();
    }

    else if (evt->event_id == HTTP_EVENT_ON_DATA)
    {
        if (evt->data_len >= 0)
        {
            httpBuffer.append((const char *)evt->data, evt->data_len);
        }
    }

    else if (evt->event_id == HTTP_EVENT_DISCONNECTED)
    {
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, nullptr);

        if (err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)
        {
            Failsafe::AddFailure("HTTP request failed: URL not found");
        }
    }

#ifdef UNIT_DEBUG
    else if (evt->event_id == HTTP_EVENT_ON_FINISH)
    {
        int statusCode = esp_http_client_get_status_code(evt->client);

        if (httpBuffer.length())
        {
            ESP_LOGI(TAG, "HTTP status code: %d - payload: %s", statusCode, httpBuffer.c_str());
        }

        else
        {
            ESP_LOGI(TAG, "HTTP status code: %d - empty response", statusCode);
        }
    }
#endif

    return ESP_OK;
}

void HTTP::Init()
{
    esp_http_client_config_t http_config = {
        .url = Backend::Address.c_str(),
        .cert_pem = DeviceConfig::WiFi::ServerCrt,
        .max_redirection_count = INT_MAX,
        .event_handler = httpEventHandler,
    };

    httpClient = esp_http_client_init(&http_config);
    assert(httpClient);
}

bool Request::GET()
{
    if (!WiFi::IsConnected())
    {
        return false;
    }

    ESP_LOGI(TAG, "GET request to URL: %s - payload: %s", m_URL.c_str(), m_Payload.c_str());
    UNIT_TIMER("GET request");

    esp_http_client_set_url(httpClient, m_URL.c_str());
    esp_http_client_set_method(httpClient, HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_perform(httpClient);
    esp_http_client_close(httpClient);

    if (err != ESP_OK)
    {
        Failsafe::AddFailure((std::string) "GET request failed: " + esp_err_to_name(err));
        return false;
    }

    int statusCode = esp_http_client_get_status_code(httpClient);
    if (Backend::CheckResponseFailed(httpBuffer, statusCode))
    {
        return false;
    }

    m_Response = httpBuffer;
    return true;
}

bool Request::POST()
{
    if (!WiFi::IsConnected())
    {
        return false;
    }

    ESP_LOGI(TAG, "POST request to URL: %s - payload: %s", m_URL.c_str(), m_Payload.c_str());
    UNIT_TIMER("POST request");

    esp_http_client_set_url(httpClient, m_URL.c_str());
    esp_http_client_set_method(httpClient, HTTP_METHOD_POST);
    esp_http_client_set_header(httpClient, "Content-Type", "application/json");
    esp_http_client_set_post_field(httpClient, m_Payload.c_str(), m_Payload.length());

    esp_err_t err = esp_http_client_perform(httpClient);
    esp_http_client_close(httpClient);

    if (err != ESP_OK)
    {
        Failsafe::AddFailure((std::string) "POST request failed: " + esp_err_to_name(err));
        return false;
    }

    int statusCode = esp_http_client_get_status_code(httpClient);
    if (Backend::CheckResponseFailed(httpBuffer, statusCode))
    {
        return false;
    }

    m_Response = httpBuffer;
    return true;
}

bool Request::Stream()
{
    bool status = false;

    if (WiFi::IsConnected())
    {
        static const int streamSize = 8192;
        static char streamBuffer[streamSize] = {0};

        ESP_LOGI(TAG, "Stream request to URL: %s - file: %s", m_URL.c_str(), m_Payload.c_str());
        UNIT_TIMER("Stream request");

        esp_http_client_set_url(httpClient, m_URL.c_str());
        esp_http_client_set_method(httpClient, HTTP_METHOD_POST);

        esp_err_t err = esp_http_client_open(httpClient, Helpers::GetFileSize(m_Payload.c_str()));

        if (err == ESP_OK)
        {
            FILE *file = fopen(m_Payload.c_str(), "rb");
            size_t read = 0;

            do
            {
                read = fread(streamBuffer, 1, streamSize, file);
                esp_http_client_write(httpClient, streamBuffer, read);
            } while (read > 0);

            fclose(file);
            status = true;
        }

        else
        {
            Failsafe::AddFailure(std::string("Stream request failed: ") + esp_err_to_name(err));
        }

        esp_http_client_close(httpClient);
    }

    return status;
}