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
static std::string httpPayload;

static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ERROR || evt->event_id == HTTP_EVENT_HEADERS_SENT)
    {
        httpPayload.clear();
    }

    else if (evt->event_id == HTTP_EVENT_ON_DATA)
    {
        if (evt->data_len >= 0)
        {
            httpPayload.append((const char *)evt->data, evt->data_len);
        }
    }

    else if (evt->event_id == HTTP_EVENT_DISCONNECTED)
    {
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, nullptr);

        if (err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)
        {
            Failsafe::AddFailure({.Message = "HTTP request failed: URL not found"});
        }

        else if (err == MBEDTLS_ERR_SSL_ALLOC_FAILED)
        {
            Failsafe::AddFailure({.Message = "HTTP request failed: Memory allocation failed"});
        }
    }

#ifdef UNIT_DEBUG
    else if (evt->event_id == HTTP_EVENT_ON_FINISH)
    {
        int statusCode = esp_http_client_get_status_code(evt->client);

        if (httpPayload.length())
        {
            ESP_LOGI(TAG, "HTTP status code: %d - payload: %s", statusCode, httpPayload.c_str());
        }

        else
        {
            ESP_LOGI(TAG, "HTTP status code: %d - empty response", statusCode);
        }
    }
#endif

    return ESP_OK;
}

bool HTTP::Init()
{
    esp_http_client_config_t http_config = {
        .url = Backend::Address.c_str(),
        .cert_pem = DeviceConfig::WiFi::ServerCrt,
        .max_redirection_count = INT_MAX,
        .event_handler = httpEventHandler,
    };

    httpClient = esp_http_client_init(&http_config);

    if (!httpClient)
    {
        Failsafe::AddFailure({.Message = "Network initialization failed"});

        return false;
    }

    return true;
}

bool HTTP::GET(const char *url, std::string &payload)
{
    if (!WiFi::IsConnected())
    {
        return false;
    }

    ESP_LOGI(TAG, "GET request to URL: %s", url);
    UNIT_TIMER("GET request");

    esp_http_client_set_url(httpClient, url);
    esp_http_client_set_method(httpClient, HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_perform(httpClient);
    int statusCode = esp_http_client_get_status_code(httpClient);

    esp_http_client_close(httpClient);

    if (err != ESP_OK)
    {
        Failsafe::AddFailure({.Message = "GET request failed", .Error = esp_err_to_name(err)});
        return false;
    }

    if (Backend::CheckResponseFailed(httpPayload, statusCode))
    {
        return false;
    }

    payload = httpPayload;

    return true;
}

bool HTTP::POST(const char *url, std::string &payload)
{
    if (!WiFi::IsConnected())
    {
        return false;
    }

    ESP_LOGI(TAG, "POST request to URL: %s - payload: %s", url, payload.c_str());
    UNIT_TIMER("POST request");

    esp_http_client_set_url(httpClient, url);
    esp_http_client_set_method(httpClient, HTTP_METHOD_POST);
    esp_http_client_set_header(httpClient, "Content-Type", "application/json");
    esp_http_client_set_post_field(httpClient, payload.c_str(), payload.length());

    esp_err_t err = esp_http_client_perform(httpClient);
    int statusCode = esp_http_client_get_status_code(httpClient);

    esp_http_client_close(httpClient);

    if (err != ESP_OK)
    {
        Failsafe::AddFailure({.Message = "POST request failed", .Error = esp_err_to_name(err)});
        return false;
    }

    if (Backend::CheckResponseFailed(httpPayload, statusCode))
    {
        return false;
    }

    payload = httpPayload;

    return true;
}

bool HTTP::Stream(const char *url, const char *filePath)
{
    bool status = false;
    static const int streamSize = 8192;
    static uint8_t streamBuffer[streamSize];

    if (WiFi::IsConnected())
    {
        esp_http_client_set_url(httpClient, url);
        esp_http_client_set_method(httpClient, HTTP_METHOD_POST);

        ESP_LOGI(TAG, "Stream request to URL: %s - File: %s", url, filePath);
        UNIT_TIMER("Stream request");

        esp_err_t err = esp_http_client_open(httpClient, Helpers::GetFileSize(filePath));

        if (err != ESP_OK)
        {
            FILE *file = fopen(filePath, "rb");
            size_t read = 0;

            do
            {
                read = fread(streamBuffer, 1, streamSize, file);
                esp_http_client_write(httpClient, (char *)streamBuffer, read);
            } while (read > 0);

            fclose(file);
            status = true;
        }

        else
        {
            Failsafe::AddFailure({.Message = "Stream request failed", .Error = esp_err_to_name(err)});
        }

        esp_http_client_close(httpClient);
    }

    return status;
}