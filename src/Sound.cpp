#include "esp_log.h"
#include "esp_http_client.h"
#include "driver/i2s_std.h"

#include "Sound.h"
#include "WavFormat.h"
#include "SoundFilter.h"
#include "Backend.h"
#include "Definitions.h"
#include "Output.h"
#include "Failsafe.h"
#include "WiFi.h"

static i2s_chan_handle_t i2sHandle;
static const char *TAG = "Sound";
static TaskHandle_t xHandle;

#ifdef UNIT_ENABLE_SOUND_RECORDING
static esp_http_client_handle_t httpClient;
static std::string httpPayload;
#endif

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing %s task", TAG);

    const i2s_std_config_t i2s_config = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
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
        .dma_desc_num = 5,
        .dma_frame_num = 2046,
        .auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &i2sHandle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2sHandle, &i2s_config));
    ESP_ERROR_CHECK(i2s_channel_enable(i2sHandle));

#ifdef UNIT_ENABLE_SOUND_RECORDING
    Backend::Address.append(Backend::SoundURL + Backend::DeviceId);
    ESP_LOGI(TAG, "Sound registering address: %s", Backend::Address.c_str());

    esp_http_client_config_t http_config = {
        .url = Backend::Address.c_str(),
        .cert_pem = DeviceConfig::WiFi::ServerCrt,
        .method = HTTP_METHOD_POST,
        .max_redirection_count = INT_MAX,
    };

    httpClient = esp_http_client_init(&http_config);
    assert(httpClient);
#endif

    for (;;)
    {
        Sound::Update();
    }

    vTaskDelete(nullptr);
}

void Sound::Init()
{
    xTaskCreate(&vTask, TAG, 8192, nullptr, configMAX_PRIORITIES - 3, &xHandle);
}

void Sound::Update()
{
    if (WiFi::IsConnected())
    {

#ifdef UNIT_ENABLE_SOUND_REGISTERING
        static clock_t previousTime = 0;
        clock_t currentTime = clock();

        ReadSound();
        if ((currentTime - previousTime) > Backend::RegisterInterval * 1000)
        {
            previousTime = currentTime;

            if (Backend::RegisterReadings())
            {
                ResetLevels();
                Output::Blink(DeviceConfig::Outputs::LedG, 1000);
            }
        }
#endif

#ifdef UNIT_ENABLE_SOUND_RECORDING
        if (ReadSound())
        {
            ESP_LOGI(TAG, "Continuing recording - dB: %d - threshold: %d", (int)m_SoundLevel, Backend::LoudnessThreshold);
            ESP_LOGI(TAG, "Post request to URL: %s - size: %ld", Backend::Address.c_str(), AudioFile::TotalLength);
            UNIT_TIMER("Post request");

            esp_err_t err = esp_http_client_open(httpClient, AudioFile::TotalLength);
            if (err != ESP_OK)
            {
                Failsafe::AddFailure("Registering sound recording failed");
                return;
            }

            Output::Blink(DeviceConfig::Outputs::LedG, 250, true);
            int length = esp_http_client_write(httpClient, (char *)&AudioFile::Header, sizeof(WaveHeader));
            if (length < 0)
            {
                Failsafe::AddFailure("Writing wav header failed");
                esp_http_client_close(httpClient);
                return;
            }

            i2s_adc_data_scale((uint8_t *)AudioFile::Buffer, (uint8_t *)AudioFile::Buffer, AudioFile::BufferLength);
            length = esp_http_client_write(httpClient, (char *)AudioFile::Buffer, AudioFile::BufferLength);
            if (length < 0)
            {
                Failsafe::AddFailure("Writing wav bytes failed");
                esp_http_client_close(httpClient);
                return;
            }

            else if (length < AudioFile::BufferLength)
            {
                ESP_LOGE(TAG, "Writing partially complete - buffer length: %ld - transfered length: %d", AudioFile::BufferLength, length);
                esp_http_client_close(httpClient);
                return;
            }

            for (size_t byte_count = AudioFile::Header.DataLength - AudioFile::BufferLength; byte_count > 0; byte_count -= AudioFile::BufferLength)
            {
                i2s_channel_read(i2sHandle, AudioFile::Buffer, AudioFile::BufferLength, nullptr, portMAX_DELAY);
                i2s_adc_data_scale((uint8_t *)AudioFile::Buffer, (uint8_t *)AudioFile::Buffer, AudioFile::BufferLength);

                length = esp_http_client_write(httpClient, (char *)AudioFile::Buffer, AudioFile::BufferLength);
                if (length < 0)
                {
                    Failsafe::AddFailure("Writing wav bytes failed");
                    esp_http_client_close(httpClient);
                    return;
                }

                else if (length < AudioFile::BufferLength)
                {
                    ESP_LOGE(TAG, "Writing partially complete - buffer length: %ld - transfered length: %d", AudioFile::BufferLength, length);
                    esp_http_client_close(httpClient);
                    return;
                }
            }

            ESP_LOGI(TAG, "Writing wav bytes complete");

            length = esp_http_client_fetch_headers(httpClient);
            if (length >= 0)
            {
                int statusCode = esp_http_client_get_status_code(httpClient);
                if (Backend::StatusOK(statusCode))
                {
                    ESP_LOGI(TAG, "Backend response ok - status code: %d", statusCode);
                }

                else
                {
                    httpPayload.clear();
                    httpPayload.resize(length);

                    length = esp_http_client_read(httpClient, httpPayload.data(), length);
                    esp_http_client_close(httpClient);
                    if (length > 0)
                    {
                        httpPayload[length] = 0;
                        Failsafe::AddFailure("Backend response failed - status code: " + std::to_string(statusCode) + " - error: " + httpPayload);
                    }

                    else
                    {
                        Failsafe::AddFailure("Backend response failed - status code: " + statusCode);
                    }
                }
            }

            else
            {
                Failsafe::AddFailure("Error fetching data from backend");
                esp_http_client_close(httpClient);
            }

            Output::SetContinuity(DeviceConfig::Outputs::LedG, false);
        }
#endif
    }
}

bool Sound::ReadSound()
{
    i2s_channel_read(i2sHandle, AudioFile::Buffer, AudioFile::BufferLength, nullptr, portMAX_DELAY);

    double rms = SoundFilter::CalculateRMS(AudioFile::Buffer, AudioFile::BufferCount);
    double decibel = 20.0f * log10(rms / MicInfo::Amplitude) + MicInfo::RefDB + MicInfo::OffsetDB;

    if (decibel > MicInfo::FloorDB && decibel < MicInfo::PeakDB)
    {
        m_SoundLevel = decibel;

        if (m_SoundLevel > m_MaxLevel)
        {
            m_MaxLevel = m_SoundLevel;
        }

        if (m_SoundLevel < m_MinLevel)
        {
            m_MinLevel = m_SoundLevel;
        }

        ESP_LOGI(TAG, "dB: %d", (int)m_SoundLevel);

        if (m_SoundLevel > Backend::LoudnessThreshold)
        {
            return true;
        }

        return false;
    }

    Failsafe::AddFailure("Sound value not valid - dB: " + std::to_string(decibel));
    return false;
}