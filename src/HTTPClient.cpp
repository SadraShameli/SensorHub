#include "esp_http_client.h"
#include "esp_tls.h"
#include "ArduinoJson.h"
#include "Definitions.h"
#include "Backend.h"
#include "Configuration.h"
#include "Failsafe.h"
#include "Storage.h"
#include "WiFi.h"
#include "HTTP.h"

namespace HTTP
{
    static const char *TAG = "HTTP Client";
    static esp_http_client_handle_t httpClient = nullptr;

    static std::string httpBuffer;

    static esp_err_t httpEventHandler(esp_http_client_event_t *evt)
    {
        if (evt->event_id == HTTP_EVENT_ERROR || evt->event_id == HTTP_EVENT_HEADERS_SENT)
            httpBuffer.clear();

        else if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            if (evt->data_len >= 0)
                httpBuffer.append((const char *)evt->data, evt->data_len);
        }

#ifdef UNIT_DEBUG
        else if (evt->event_id == HTTP_EVENT_ON_FINISH)
        {
            int statusCode = esp_http_client_get_status_code(evt->client);

            if (httpBuffer.length())
                ESP_LOGI(TAG, "Status: %d - %s", statusCode, httpBuffer.c_str());
            else
                ESP_LOGI(TAG, "Status: %d - empty response", statusCode);
        }
#endif

        return ESP_OK;
    }

    void Init()
    {
        esp_http_client_config_t http_config = {
            .url = Storage::GetAddress().c_str(),
            .max_redirection_count = INT_MAX,
            .event_handler = httpEventHandler,
        };

        httpClient = esp_http_client_init(&http_config);
        assert(httpClient);
    }

    bool StatusOK(int statusCode)
    {
        if (statusCode > 99 && statusCode < 400)
            return true;
        return false;
    }

    bool IsRedirect(int statusCode)
    {
        if (statusCode == 300 || statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308)
            return true;
        return false;
    }

    bool Request::GET()
    {
        if (!WiFi::IsConnected())
            return false;

        ESP_LOGI(TAG, "GET request to URL: %s", m_URL.c_str());
        UNIT_TIMER("GET request");

        esp_http_client_set_url(httpClient, m_URL.c_str());
        esp_http_client_set_method(httpClient, HTTP_METHOD_GET);

        esp_err_t err = esp_http_client_perform(httpClient);
        esp_http_client_close(httpClient);

        if (err != ESP_OK)
        {
            Failsafe::AddFailure(TAG, "GET request failed - " + (err == ESP_ERR_HTTP_CONNECT ? "URL not found: " + m_URL : esp_err_to_name(err)));
            return false;
        }

        int statusCode = esp_http_client_get_status_code(httpClient);
        if (Backend::CheckResponseFailed(httpBuffer, statusCode))
            return false;

        m_Response = std::move(httpBuffer);
        return true;
    }

    bool Request::POST(const std::string &payload)
    {
        if (!WiFi::IsConnected())
            return false;

        ESP_LOGI(TAG, "POST request to URL: %s - payload: %s", m_URL.c_str(), payload.c_str());
        UNIT_TIMER("POST request");

        esp_http_client_set_url(httpClient, m_URL.c_str());
        esp_http_client_set_method(httpClient, HTTP_METHOD_POST);
        esp_http_client_set_header(httpClient, "Content-Type", "application/json");
        esp_http_client_set_post_field(httpClient, payload.c_str(), payload.length());

        esp_err_t err = esp_http_client_perform(httpClient);
        esp_http_client_close(httpClient);

        if (err != ESP_OK)
        {
            Failsafe::AddFailure(TAG, "POST request failed - " + (err == ESP_ERR_HTTP_CONNECT ? "URL not found: " + m_URL : esp_err_to_name(err)));
            return false;
        }

        int statusCode = esp_http_client_get_status_code(httpClient);
        if (Backend::CheckResponseFailed(httpBuffer, statusCode))
            return false;

        m_Response = std::move(httpBuffer);
        return true;
    }

    bool Request::Stream(const char *filename)
    {
        bool status = false;

        if (WiFi::IsConnected())
        {
            static const int streamSize = 8192;
            static char streamBuffer[streamSize] = {0};

            ESP_LOGI(TAG, "Stream request to URL: %s - file: %s", m_URL.c_str(), filename);
            UNIT_TIMER("Stream request");

            esp_http_client_set_url(httpClient, m_URL.c_str());
            esp_http_client_set_method(httpClient, HTTP_METHOD_POST);

            esp_err_t err = esp_http_client_open(httpClient, Helpers::GetFileSize(filename));
            if (err == ESP_OK)
            {
                FILE *file = fopen(filename, "rb");
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
                Failsafe::AddFailure(TAG, "Stream request failed - " + (err == ESP_ERR_HTTP_CONNECT ? "URL not found: " + m_URL : esp_err_to_name(err)));
            esp_http_client_close(httpClient);
        }

        return status;
    }
}