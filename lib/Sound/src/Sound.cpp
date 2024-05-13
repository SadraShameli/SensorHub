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

#define MIC_OFFSET_DB -3.0f
#define MIC_SENSITIVITY -26
#define MIC_REF_DB 94.0f
#define MIC_OVERLOAD_DB 120.0f
#define MIC_NOISE_DB 29

#define SAMPLE_TYPE int16_t
#define SAMPLE_BITRATE (sizeof(SAMPLE_TYPE) * 8)

#ifdef UNIT_ENABLE_SOUND_RECORDING
#define SAMPLE_RATE 48000
#define RECORD_TIME 5000
#define BUFFER_TIME 500

#elif defined(UNIT_ENABLE_SOUND_REGISTERING)
#define SAMPLE_RATE 16000
#define RECORD_TIME 1000
#endif

static i2s_chan_handle_t i2sHandle;
static const double micAmplitude = pow(10, MIC_SENSITIVITY / 20.0f) * ((1 << (SAMPLE_BITRATE - 1)) - 1);
static wave_audio_t<SAMPLE_TYPE, SAMPLE_RATE, BUFFER_TIME> wave(RECORD_TIME);

static esp_http_client_handle_t httpClient;
static std::string httpPayload;

static const char *TAG = "Sound";
static TaskHandle_t xHandle;

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
        .dma_desc_num = 12,
        .dma_frame_num = 2046,
        .auto_clear = true,
    };

    esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &i2sHandle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2S channel allocation failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
    }

    err = i2s_channel_init_std_mode(i2sHandle, &i2s_config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2S channel initialization failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
    }

    err = i2s_channel_enable(i2sHandle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2S channel enabling failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
    }

#ifdef UNIT_ENABLE_SOUND_RECORDING
    Backend::Address.append(Backend::SoundURL + Backend::DeviceId);

    esp_http_client_config_t http_config = {
        .url = Backend::Address.c_str(),
        .cert_pem = DeviceConfig::WiFi::ServerCrt,
        .method = HTTP_METHOD_POST,
        .max_redirection_count = INT_MAX,
    };

    httpClient = esp_http_client_init(&http_config);

    if (!httpClient)
    {
        ESP_LOGE(TAG, "Sound network initialization failed");
        vTaskDelete(nullptr);
    }
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
    while (WiFi::IsConnected())
    {
        Output::SetContinuity(DeviceConfig::LedY, false);

        size_t i2sBytesRead = 0;
        i2s_channel_read(i2sHandle, wave.buffer, wave.buffer_length, &i2sBytesRead, portMAX_DELAY);

#ifdef UNIT_ENABLE_SOUND_REGISTERING
        static clock_t previousTime = 0;
        clock_t currentTime = clock();
#endif

        double rms = SoundFilter::CalculateRMS(wave.buffer, wave.buffer_count);
        double decibel = 20.0f * log10(rms / micAmplitude) + MIC_REF_DB + MIC_OFFSET_DB;

        if (decibel >= MIC_NOISE_DB && decibel <= MIC_OVERLOAD_DB)
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

#ifdef UNIT_ENABLE_SOUND_REGISTERING
            if ((currentTime - previousTime) > Backend::RegisterInterval * 1000)
            {
                previousTime = currentTime;

                if (Backend::RegisterReadings())
                {
                    Output::Blink(DeviceConfig::Outputs::LedG, 1000);
                }

                else
                {
                    Output::Blink(DeviceConfig::Outputs::LedR, 1000);
                }

                ResetLevels();
            }
#endif

#ifdef UNIT_ENABLE_SOUND_RECORDING
            if (m_SoundLevel > Backend::LoudnessThreshold)
            {
                ESP_LOGI(TAG, "Continuing recording - dB: %f - threshold: %d", m_SoundLevel, Backend::LoudnessThreshold);
                ESP_LOGI(TAG, "Post request to URL: %s - size: %u", Backend::Address.c_str(), wave.total_size);
                UNIT_TIMER("Stream request");

                esp_err_t err = esp_http_client_open(httpClient, wave.total_size);

                if (err == ESP_OK)
                {
                    Output::Blink(DeviceConfig::Outputs::LedG, 250, true);

                    esp_http_client_write(httpClient, (char *)&wave.header, sizeof(wave_header_t));

                    for (size_t byte_count = wave.header.data_length; byte_count > 0; byte_count -= wave.buffer_length)
                    {
                        i2s_channel_read(i2sHandle, wave.buffer, wave.buffer_length, &i2sBytesRead, portMAX_DELAY);
                        esp_http_client_write(httpClient, (char *)wave.buffer, wave.buffer_length);
                    }

                    int length = esp_http_client_fetch_headers(httpClient);
                    int statusCode = esp_http_client_get_status_code(httpClient);

                    Output::SetContinuity(DeviceConfig::Outputs::LedG, false);

                    if (!Backend::StatusOK(statusCode))
                    {
                        if (length > 0)
                        {
                            httpPayload.clear();
                            httpPayload.resize(length);

                            esp_http_client_read_response(httpClient, httpPayload.data(), httpPayload.capacity());

                            Failsafe::AddFailure({.Message = "Backend response failed - status code: " + std::to_string(statusCode) + " - error", .Error = httpPayload});
                        }

                        else
                        {
                            Failsafe::AddFailure({.Message = "Backend response failed - status code: " + statusCode});
                        }
                    }
                }

                else
                {
                    Failsafe::AddFailure({.Message = "Registering sound recording failed"});
                }

                esp_http_client_close(httpClient);
            }
#endif
        }

        else
        {
            Failsafe::AddFailure({.Message = "Sound value not valid - dB: " + std::to_string(decibel)});
        }
    }

    Output::Blink(DeviceConfig::LedY, 250, true);
    vTaskDelay(pdMS_TO_TICKS(25));
}

double Sound::GetLevel()
{
    return m_SoundLevel;
}

double Sound::GetMaxLevel()
{
    return m_MaxLevel;
}

double Sound::GetMinLevel()
{
    return m_MinLevel;
}

void Sound::ResetLevels()
{
    m_MaxLevel = m_MinLevel = m_SoundLevel;
}