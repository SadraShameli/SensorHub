#include "driver/i2s_std.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_log.h"
#include "WavFormat.h"
#include "SoundFilter.h"
#include "Configuration.h"
#include "Backend.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Output.h"
#include "Display.h"
#include "WiFi.h"
#include "Sound.h"

namespace Sound
{
    namespace Constants
    {
        static const float Sensitivity = 26.0f, RefDB = 94.0f, OffsetDB = -3.0f, PeakDB = 116.0f, FloorDB = 29.0f;
        static const double Amplitude = std::pow(10, -Sensitivity / 20.0f) * ((1 << (16 - 1)) - 1);
        static const float LoudnessOffset = 0;
    };

    static const char *TAG = "Sound";
    static TaskHandle_t xHandle = nullptr;
    static i2s_chan_handle_t i2sHandle;
    static esp_http_client_handle_t httpClient;

    static AudioFile *audio;
    static Reading loudness;
    static std::string address, httpPayload;
    static uint32_t transferLength = 0, transferCount = 0;
    static bool isOK = false;

    static void vTask(void *arg)
    {
        ESP_LOGI(TAG, "Initializing");

        if (Storage::GetSensorState(Configuration::Sensor::Recording))
        {
            audio = new AudioFile(48000, 16, 1000, 10);
            assert(audio);

            transferLength = audio->BufferLength / 8;
            transferCount = transferLength / audio->Header.BytesPerSample;
            address = Storage::GetAddress() + Backend::RecordingURL + std::to_string(Storage::GetDeviceId());
            ESP_LOGI(TAG, "Recording register address: %s", address.c_str());

            esp_http_client_config_t http_config = {
                .url = address.c_str(),
                .method = HTTP_METHOD_POST,
                .max_redirection_count = INT_MAX,
            };

            httpClient = esp_http_client_init(&http_config);
            assert(httpClient);
        }

        else
        {
            audio = new AudioFile(16000, 16, 125, 0);
            assert(audio);
        }

        const i2s_std_config_t i2s_config = {
            .clk_cfg = {
                .sample_rate_hz = audio->Header.SampleRate,
                .clk_src = I2S_CLK_SRC_APLL,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .bclk = GPIO_NUM_23,
                .ws = GPIO_NUM_18,
                .dout = I2S_GPIO_UNUSED,
                .din = GPIO_NUM_19,
                .invert_flags = {
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

        for (int i = 0; i < 4; i++)
            ESP_ERROR_CHECK(i2s_channel_read(i2sHandle, audio->Buffer, transferLength, nullptr, portMAX_DELAY));

        double rms = SoundFilter::CalculateRMS((int16_t *)audio->Buffer, transferCount);
        double decibel = 20.0f * log10(rms / Constants::Amplitude) + Constants::RefDB + Constants::OffsetDB;
        if (decibel > Constants::FloorDB && decibel < Constants::PeakDB)
        {
            loudness.Update((float)decibel + Constants::LoudnessOffset);
            isOK = true;
        }

        else
        {
            Failsafe::AddFailure(TAG, "No mic detected");
            vTaskDelete(nullptr);
        }

        if (Storage::GetSensorState(Configuration::Sensor::Recording))
            for (;;)
                UpdateRecording();
        else
            for (;;)
                Update();

        vTaskDelete(nullptr);
    }

    void Init()
    {
        xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 4, &xHandle);
    }

    void Update()
    {
        UpdateLoudness();
    }

    void UpdateRecording()
    {
        if (UpdateLoudness() && WiFi::IsConnected())
        {
            ESP_LOGI(TAG, "Continuing recording - loudness: %ddB - threshold: %lddB", (int)loudness.Current(), Storage::GetLoudnessThreshold());
            ESP_LOGI(TAG, "POST request to URL: %s - size: %ld", address.c_str(), audio->TotalLength);
            UNIT_TIMER("POST request");

            esp_err_t err = esp_http_client_open(httpClient, audio->TotalLength);
            if (err != ESP_OK)
            {
                Failsafe::AddFailure(TAG, "POST request failed: " + (err == ESP_ERR_HTTP_CONNECT ? "URL not found: " + address : esp_err_to_name(err)));
                return;
            }

            Output::Blink(Output::LedG, 250, true);
            RegisterRecordings();
            Output::SetContinuity(Output::LedG, false);
        }
    }

    bool UpdateLoudness()
    {
        i2s_channel_read(i2sHandle, audio->Buffer, audio->BufferLength, nullptr, portMAX_DELAY);
        double decibel = CalculateLoudness();
        if (!decibel)
        {
            Failsafe::AddFailureDelayed(TAG, "Loudness not valid: " + std::to_string(decibel) + "dB");
            return isOK = false;
        }

        isOK = true;
        loudness.Update(decibel + Constants::LoudnessOffset);
        if (loudness.Current() > Storage::GetLoudnessThreshold())
            return true;

        return false;
    }

    void RegisterRecordings()
    {
        int length = esp_http_client_write(httpClient, (char *)&audio->Header, sizeof(WaveHeader));
        if (length < 0)
        {
            Failsafe::AddFailure(TAG, "Writing wav header failed");
            esp_http_client_close(httpClient);
            return;
        }

        i2s_adc_data_scale(audio->Buffer, audio->Buffer, audio->BufferLength);
        for (size_t byte_count = 0; byte_count < audio->BufferLength; byte_count += transferLength)
        {
            length = esp_http_client_write(httpClient, (char *)audio->Buffer + byte_count, transferLength);
            if (length < 0)
            {
                Failsafe::AddFailure(TAG, "Writing data failed");
                esp_http_client_close(httpClient);
                return;
            }

            else if (length < transferLength)
            {
                Failsafe::AddFailure(TAG, "Writing data partially complete");
                esp_http_client_close(httpClient);
                return;
            }
        }

        for (size_t byte_count = 0; byte_count < audio->Header.DataLength; byte_count += transferLength)
        {
            i2s_channel_read(i2sHandle, audio->Buffer, transferLength, nullptr, portMAX_DELAY);

            if (Display::IsOK())
            {
                double rms = SoundFilter::CalculateRMS((int16_t *)audio->Buffer, transferCount);
                double decibel = 20.0f * log10(rms / Constants::Amplitude) + Constants::RefDB + Constants::OffsetDB;
                if (decibel > Constants::FloorDB && decibel < Constants::PeakDB)
                    loudness.Update((float)decibel);
            }

            i2s_adc_data_scale(audio->Buffer, audio->Buffer, transferLength);
            length = esp_http_client_write(httpClient, (char *)audio->Buffer, transferLength);
            if (length < 0)
            {
                Failsafe::AddFailure(TAG, "Writing data failed");
                esp_http_client_close(httpClient);
                return;
            }

            else if (length < transferLength)
            {
                Failsafe::AddFailure(TAG, "Writing data partially complete");
                esp_http_client_close(httpClient);
                return;
            }
        }

        length = esp_http_client_fetch_headers(httpClient);
        if (length < 0)
        {
            Failsafe::AddFailure(TAG, "Fetching backend response failed");
            esp_http_client_close(httpClient);
            return;
        }

        int statusCode = esp_http_client_get_status_code(httpClient);
        if (esp_http_client_is_chunked_response(httpClient))
        {
            httpPayload.clear();
            httpPayload.resize(DEFAULT_HTTP_BUF_SIZE);
        }

        else if (length > 0)
        {
            httpPayload.clear();
            httpPayload.resize(length);
        }

        else
        {
            Failsafe::AddFailure(TAG, "Error fetching data from backend - status code: " + statusCode);
            esp_http_client_close(httpClient);
            return;
        }

        esp_http_client_read(httpClient, httpPayload.data(), httpPayload.capacity());
        esp_http_client_close(httpClient);

        Backend::CheckResponseFailed(httpPayload, statusCode);
    }

    float CalculateLoudness()
    {
        double rms = SoundFilter::CalculateRMS((int16_t *)audio->Buffer, audio->BufferCount);
        double decibel = 20.0f * log10(rms / Constants::Amplitude) + Constants::RefDB + Constants::OffsetDB;

        // ESP_LOGI(TAG, "Loudness: %fdB", decibel);

        if (decibel > Constants::FloorDB && decibel < Constants::PeakDB)
            return (float)decibel;
        return 0;
    }

    bool IsOK() { return isOK; };
    void ResetValues() { loudness.Reset(); };
    const Reading &GetLoudness() { return loudness; }
}
