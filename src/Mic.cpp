#include "Mic.h"

#include "Audio.h"
#include "Backend.h"
#include "Definitions.h"
#include "Display.h"
#include "Failsafe.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"
#include "driver/i2s_std.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"

namespace Mic {
namespace Constants {

static const float Sensitivity = 26.0f, RefDB = 94.0f, OffsetDB = -3.0f,
                   PeakDB = 116.0f, FloorDB = 29.0f;

static const float Amplitude =
    powf(10.f, -Sensitivity / 20.0f) * ((1 << (16 - 1)) - 1);

static const float LoudnessOffset = 0;

};  // namespace Constants

static const char* TAG = "Sound";
static TaskHandle_t xHandle = nullptr;

static i2s_chan_handle_t i2sHandle = nullptr;
static esp_http_client_handle_t httpClient = nullptr;

static Audio* audio = nullptr;
static Reading loudness;
static bool isOK = false;

static std::string address, httpPayload;
static uint32_t transferLength = 0, transferCount = 0;

/**
 * @brief Task function to initialize and manage audio recording or monitoring.
 *
 * This function initializes the audio system based on the sensor state. If the
 * sensor is in recording mode, it sets up the audio system for recording with
 * specific configurations and initializes an HTTP client for data transfer. If
 * the sensor is not in recording mode, it sets up the audio system for
 * monitoring.
 *
 * The function configures the I2S interface for audio data transfer and reads
 * initial audio data to calculate the RMS and decibel levels. If the decibel
 * level is within the acceptable range, it updates the loudness and continues
 * to either record or monitor audio data in an infinite loop. If the decibel
 * level is not within the acceptable range, it logs a failure and deletes the
 * task.
 *
 * @param arg Pointer to the task argument (unused).
 */
static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    if (Storage::GetSensorState(Configuration::Sensor::Recording)) {
        audio = new Audio(48000, 16, 1000, 10);
        transferLength = audio->BufferLength / 8;
        transferCount = transferLength / audio->Header.BytesPerSample;

        address = Storage::GetAddress() + Backend::RecordingURL +
                  std::to_string(Storage::GetDeviceId());

        ESP_LOGI(TAG, "Recording register address: %s", address.c_str());

        esp_http_client_config_t httpConfig = {
            .url = address.c_str(),
            .method = HTTP_METHOD_POST,
            .max_redirection_count = INT_MAX,
        };

        httpClient = esp_http_client_init(&httpConfig);
        assert(httpClient);
    }

    else {
        audio = new Audio(16000, 16, 125, 0);
        transferLength = audio->BufferLength;
        transferCount = audio->BufferCount;
    }

    const i2s_std_config_t i2s_config = {
        .clk_cfg =
            {
                .sample_rate_hz = audio->Header.SampleRate,
                .clk_src = I2S_CLK_SRC_APLL,
                .mclk_multiple = I2S_MCLK_MULTIPLE_512,
            },
        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .bclk = GPIO_NUM_23,
                .ws = GPIO_NUM_18,
                .dout = I2S_GPIO_UNUSED,
                .din = GPIO_NUM_19,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };

    const i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = audio->DMA_DescNum,
        .dma_frame_num = audio->DMA_FrameNum,
        .auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &i2sHandle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2sHandle, &i2s_config));
    ESP_ERROR_CHECK(i2s_channel_enable(i2sHandle));

    if (Storage::GetSensorState(Configuration::Sensor::Recording)) {
        for (int i = 0; i < 4; i++) {
            ESP_ERROR_CHECK(i2s_channel_read(i2sHandle,
                                             audio->Buffer,
                                             transferLength,
                                             nullptr,
                                             portMAX_DELAY));
        }
    }

    else {
        for (int i = 0; i < 6; i++) {
            ESP_ERROR_CHECK(i2s_channel_read(i2sHandle,
                                             audio->Buffer,
                                             transferLength,
                                             nullptr,
                                             portMAX_DELAY));
        }
    }

    float rms = CalculateRMS((int16_t*)audio->Buffer, transferCount);
    float decibel = 20.0f * log10f(rms / Constants::Amplitude) +
                    Constants::RefDB + Constants::OffsetDB;

    if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
        loudness.Update(decibel + Constants::LoudnessOffset);
        isOK = true;
    }

    else {
        Failsafe::AddFailure(TAG, "No mic detected");
        vTaskDelete(nullptr);
    }

    if (Storage::GetSensorState(Configuration::Sensor::Recording)) {
        for (;;) {
            UpdateRecording();
        }
    }

    else {
        for (;;) {
            Update();
        }
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Initializes the microphone task.
 *
 * This function creates a new FreeRTOS task for handling microphone operations.
 * The task is created with a stack size of 8192 bytes and a priority of
 * `tskIDLE_PRIORITY + 4`. The task handle is stored in the variable `xHandle`.
 */
void Init() {
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 4, &xHandle);
}

/**
 * @brief Updates the loudness and continues recording if the loudness is above
 * the threshold.
 *
 * This function updates the loudness by calculating the RMS and decibel levels
 * of the audio data. If the loudness is above the threshold, it continues
 * recording the audio data by sending a POST request to the backend.
 *
 * @return `true` if the loudness is above the threshold, `false` otherwise.
 */
void Update() {
    UpdateLoudness();
}

/**
 * @brief Updates the loudness and continues recording if the loudness is above
 * the threshold.
 *
 * This function updates the loudness by calculating the RMS and decibel levels
 * of the audio data. If the loudness is above the threshold, it continues
 * recording the audio data by sending a POST request to the backend.
 *
 * @return `true` if the loudness is above the threshold, `false` otherwise.
 */
void UpdateRecording() {
    if (!UpdateLoudness() || !WiFi::IsConnected()) {
        return;
    }

    ESP_LOGI(TAG,
             "Continuing recording - loudness: %ddB - threshold: %lddB",
             (int)loudness.Current(),
             Storage::GetLoudnessThreshold());

    ESP_LOGI(TAG,
             "POST request to URL: %s - size: %ld",
             address.c_str(),
             audio->TotalLength);

    UNIT_TIMER("POST request");

    esp_err_t err = esp_http_client_open(httpClient, audio->TotalLength);

    if (err != ESP_OK) {
        Failsafe::AddFailure(
            TAG,
            "POST request failed - " + (err == ESP_ERR_HTTP_CONNECT
                                            ? "URL not found: " + address
                                            : esp_err_to_name(err)));

        return;
    }

    Output::Blink(Output::LedG, 250, true);

    RegisterRecordings();
    esp_http_client_close(httpClient);

    Output::SetContinuity(Output::LedG, false);
}

/**
 * @brief Updates the loudness measurement from the microphone.
 *
 * This function reads audio data from the I2S channel, calculates the loudness
 * in decibels, and updates the loudness value. If the calculated loudness is
 * not valid, it logs a failure and sets the status to not OK. If the loudness
 * exceeds a predefined threshold, the function returns true.
 *
 * @return `true` if the loudness exceeds the threshold, `false` otherwise.
 */
bool UpdateLoudness() {
    i2s_channel_read(i2sHandle,
                     audio->Buffer,
                     audio->BufferLength,
                     nullptr,
                     portMAX_DELAY);

    float decibel = CalculateLoudness();

    if (decibel == 0) {
        Failsafe::AddFailureDelayed(
            TAG,
            "Loudness not valid: " + std::to_string(decibel) + "dB");

        return isOK = false;
    }

    isOK = true;
    loudness.Update(decibel + Constants::LoudnessOffset);

    if ((uint32_t)loudness.Current() > Storage::GetLoudnessThreshold()) {
        return true;
    }

    return false;
}

/**
 * @brief Registers audio recordings by sending them to a backend server.
 *
 * This function writes the WAV header and audio data to an HTTP client.
 * It reads audio data from an I2S channel, calculates the RMS and decibel
 * levels, and updates the loudness if within a specified range. It also
 * handles HTTP responses and checks for failures.
 */
void RegisterRecordings() {
    int length = esp_http_client_write(httpClient,
                                       (char*)&audio->Header,
                                       sizeof(WavHeader));

    if (length < 0) {
        Failsafe::AddFailure(TAG, "Writing wav header failed");
        return;
    }

    for (size_t byte_count = 0; byte_count < audio->BufferLength;
         byte_count += transferLength) {
        length = esp_http_client_write(httpClient,
                                       (char*)audio->Buffer + byte_count,
                                       transferLength);

        if (length < 0) {
            Failsafe::AddFailure(TAG, "Writing data failed");
            return;
        }

        else if (length < transferLength) {
            Failsafe::AddFailure(TAG, "Writing data partially complete");
            return;
        }
    }

    for (size_t byte_count = 0;
         byte_count < audio->Header.DataLength - audio->BufferLength;
         byte_count += transferLength) {
        i2s_channel_read(i2sHandle,
                         audio->Buffer,
                         transferLength,
                         nullptr,
                         portMAX_DELAY);

        if (Display::IsOK()) {
            float rms = CalculateRMS((int16_t*)audio->Buffer, transferCount);
            float decibel = 20.0f * log10f(rms / Constants::Amplitude) +
                            Constants::RefDB + Constants::OffsetDB;

            if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
                loudness.Update(decibel);
            }
        }

        length = esp_http_client_write(httpClient,
                                       (char*)audio->Buffer,
                                       transferLength);

        if (length < 0) {
            Failsafe::AddFailure(TAG, "Writing data failed");
            return;
        }

        else if (length < transferLength) {
            Failsafe::AddFailure(TAG, "Writing data partially complete");
            return;
        }
    }

    length = (int)esp_http_client_fetch_headers(httpClient);
    if (length < 0) {
        Failsafe::AddFailure(TAG, "Fetching backend response failed");
        return;
    }

    int statusCode = esp_http_client_get_status_code(httpClient);
    if (esp_http_client_is_chunked_response(httpClient)) {
        httpPayload.clear();
        httpPayload.resize(DEFAULT_HTTP_BUF_SIZE);
    }

    else if (length > 0) {
        httpPayload.clear();
        httpPayload.resize(length);
    }

    else {
        Failsafe::AddFailure(
            TAG,
            "Status: " + std::to_string(statusCode) + " - empty response");
        return;
    }

    esp_http_client_read(httpClient,
                         httpPayload.data(),
                         httpPayload.capacity());

    if (!Backend::CheckResponseFailed(httpPayload,
                                      (HTTP::Status::StatusCode)statusCode)) {
        ResetValues();
    }
}

/**
 * @brief Calculates the loudness in decibels (dB) from the audio buffer.
 *
 * This function computes the Root Mean Square (RMS) value of the audio buffer,
 * converts it to decibels, and logs the loudness. If the calculated decibel
 * value is within the specified floor and peak dB range, it returns the decibel
 * value. Otherwise, it returns 0.
 *
 * @return The loudness in decibels if within the valid range, otherwise 0.
 */
float CalculateLoudness() {
    float rms = CalculateRMS((int16_t*)audio->Buffer, audio->BufferCount);
    float decibel = 20.0f * log10f(rms / Constants::Amplitude) +
                    Constants::RefDB + Constants::OffsetDB;

    ESP_LOGD(TAG, "Loudness: %ddB", (int)decibel);

    if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
        return decibel;
    }

    return 0;
}

/**
 * @brief Checks if the microphone is functioning correctly.
 *
 * @return `true` if the microphone is OK, `false` otherwise.
 */
bool IsOK() {
    return isOK;
};

/**
 * @brief Resets the loudness value.
 */
void ResetValues() {
    loudness.Reset();
};

/**
 * @brief Gets the loudness value.
 *
 * @return Reading The loudness value.
 */
const Reading& GetLoudness() {
    return loudness;
}

}  // namespace Mic
