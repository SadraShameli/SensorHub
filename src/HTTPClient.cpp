#include "ArduinoJson.h"
#include "Backend.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "HTTP.h"
#include "Storage.h"
#include "WiFi.h"
#include "esp_http_client.h"
#include "esp_tls.h"

namespace HTTP {

static const char* TAG = "HTTP Client";

static esp_http_client_handle_t httpClient = nullptr;
static std::string httpBuffer;

/**
 * @brief HTTP event handler for processing HTTP client events.
 *
 * This function handles various HTTP client events such as errors, headers
 * sent, data received, and request completion. It processes the events and
 * updates the HTTP buffer accordingly.
 *
 * @param evt Pointer to the HTTP client event structure.
 * @return `esp_err_t` Returns `ESP_OK` on successful handling of the event.
 *
 * @note The function clears the HTTP buffer on error or headers sent events.
 * It appends received data to the HTTP buffer on data events.
 * If `UNIT_DEBUG` is defined, it logs the status code and response on
 * request completion.
 */
static esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ERROR ||
        evt->event_id == HTTP_EVENT_HEADERS_SENT) {
        httpBuffer.clear();
    }

    else if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (evt->data_len >= 0) {
            httpBuffer.append((const char*)evt->data, evt->data_len);
        }
    }
#ifdef UNIT_DEBUG
    else if (evt->event_id == HTTP_EVENT_ON_FINISH) {
        int statusCode = esp_http_client_get_status_code(evt->client);

        if (httpBuffer.length()) {
            ESP_LOGI(TAG, "Status: %d - %s", statusCode, httpBuffer.c_str());
        } else {
            ESP_LOGI(TAG, "Status: %d - empty response", statusCode);
        }
    }
#endif

    return ESP_OK;
}

/**
 * @brief Initializes the HTTP client with the specified configuration.
 *
 * This function sets up the HTTP client using the configuration parameters
 * provided. It retrieves the URL from the storage, sets the maximum redirection
 * count to the maximum integer value, and assigns the event handler for HTTP
 * events. The HTTP client is then initialized with these settings.
 *
 * @note This function uses the ESP-IDF HTTP client library.
 *
 * @throws An error if the HTTP client initialization fails.
 */
void Init() {
    esp_http_client_config_t httpConfig = {
        .url = Storage::GetAddress().c_str(),
        .max_redirection_count = INT_MAX,
        .event_handler = httpEventHandler,
    };

    httpClient = esp_http_client_init(&httpConfig);
    assert(httpClient);
}

/**
 * @brief Sends a GET request to the specified URL.
 *
 * This function checks if the WiFi is connected before attempting to send a GET
 * request. It logs the URL being requested and measures the time taken for the
 * request. The URL and method are set for the HTTP client, and the request is
 * performed. If the request fails, a failure is logged with the appropriate
 * error message. The function also checks the response status code and
 * processes the response.
 *
 * @return `true` if the GET request was successful and the response is valid,
 * `false` otherwise.
 */
bool Request::GET() {
    if (!WiFi::IsConnected()) {
        return false;
    }

    ESP_LOGI(TAG, "GET request to URL: %s", m_URL.c_str());
    UNIT_TIMER("GET request");

    esp_http_client_set_url(httpClient, m_URL.c_str());
    esp_http_client_set_method(httpClient, HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_perform(httpClient);
    esp_http_client_close(httpClient);

    if (err != ESP_OK) {
        Failsafe::AddFailure(
            TAG,
            "GET request failed - " + (err == ESP_ERR_HTTP_CONNECT
                                           ? "URL not found: " + m_URL
                                           : esp_err_to_name(err)));

        return false;
    }

    int statusCode = esp_http_client_get_status_code(httpClient);
    if (Backend::CheckResponseFailed(httpBuffer,
                                     (HTTP::Status::StatusCode)statusCode)) {
        return false;
    }

    m_Response = std::move(httpBuffer);
    return true;
}

/**
 * @brief Sends a POST request with the given payload to the URL specified in
 * the Request object.
 *
 * This function checks if the WiFi is connected before attempting to send the
 * request. It logs the URL and payload, sets up the HTTP client with the
 * appropriate URL, method, headers, and payload, and then performs the request.
 *
 * If the request fails, it logs the failure and returns false. If the request
 * succeeds, it checks the response status code and processes the response.
 *
 * @param payload The JSON payload to be sent in the POST request.
 * @return `true` if the POST request was successful and the response was valid,
 * `false` otherwise.
 */
bool Request::POST(const std::string& payload) {
    if (!WiFi::IsConnected()) {
        return false;
    }

    ESP_LOGI(TAG,
             "POST request to URL: %s - payload: %s",
             m_URL.c_str(),
             payload.c_str());
    UNIT_TIMER("POST request");

    esp_http_client_set_url(httpClient, m_URL.c_str());
    esp_http_client_set_method(httpClient, HTTP_METHOD_POST);
    esp_http_client_set_header(httpClient, "Content-Type", "application/json");
    esp_http_client_set_post_field(httpClient,
                                   payload.c_str(),
                                   payload.length());

    esp_err_t err = esp_http_client_perform(httpClient);
    esp_http_client_close(httpClient);

    if (err != ESP_OK) {
        Failsafe::AddFailure(
            TAG,
            "POST request failed - " + (err == ESP_ERR_HTTP_CONNECT
                                            ? "URL not found: " + m_URL
                                            : esp_err_to_name(err)));
        return false;
    }

    int statusCode = esp_http_client_get_status_code(httpClient);
    if (Backend::CheckResponseFailed(httpBuffer,
                                     (HTTP::Status::StatusCode)statusCode)) {
        return false;
    }

    m_Response = std::move(httpBuffer);
    return true;
}

/**
 * @brief Streams the content of a file to a specified URL using HTTP POST
 * method.
 *
 * This function checks if the WiFi is connected, then attempts to stream the
 * content of the specified file to the URL set in the Request object. It reads
 * the file in chunks and writes each chunk to the HTTP client. If the streaming
 * is successful, it returns `true`. Otherwise, it logs the failure and returns
 * `false`.
 *
 * @param filename The path to the file that needs to be streamed.
 * @return `true` if the file was successfully streamed, `false` otherwise.
 */
bool Request::Stream(const char* filename) {
    if (!WiFi::IsConnected()) {
        return false;
    }

    bool status = false;
    static const int streamSize = 8192;
    static char streamBuffer[streamSize] = {0};

    ESP_LOGI(TAG,
             "Stream request to URL: %s - file: %s",
             m_URL.c_str(),
             filename);
    UNIT_TIMER("Stream request");

    esp_http_client_set_url(httpClient, m_URL.c_str());
    esp_http_client_set_method(httpClient, HTTP_METHOD_POST);

    esp_err_t err =
        esp_http_client_open(httpClient, Helpers::GetFileSize(filename));

    if (err == ESP_OK) {
        FILE* file = fopen(filename, "rb");
        size_t read = 0;

        do {
            read = fread(streamBuffer, 1, streamSize, file);
            esp_http_client_write(httpClient, streamBuffer, read);
        } while (read > 0);

        fclose(file);
        status = true;
    }

    else {
        Failsafe::AddFailure(
            TAG,
            "Stream request failed - " + (err == ESP_ERR_HTTP_CONNECT
                                              ? "URL not found: " + m_URL
                                              : esp_err_to_name(err)));
    }

    esp_http_client_close(httpClient);

    return status;
}

/**
 * @brief Retrieves the URL used for the request.
 *
 * @return `const std::string&` The URL used for the request.
 */
const std::string& Request::GetURL() {
    return m_URL;
}

/**
 * @brief Retrieves the response received from the request.
 *
 * @return `const std::string&` The response received from the request.
 */
const std::string& Request::GetResponse() {
    return m_Response;
}

/**
 * @brief Removes the trailing slash from the URL if it exists.
 *
 * This function checks if the last character of the URL `m_URL` is a slash
 * `'/'`. If it is, the function removes the trailing slash from the URL.
 */
void Request::RemoveSlash() {
    if (m_URL.back() == '/') {
        m_URL.pop_back();
    }
}

}  // namespace HTTP
