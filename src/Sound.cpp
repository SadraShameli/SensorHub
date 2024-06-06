#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "driver/i2s_std.h"
#include "Sound.h"
#include "WavFormat.h"
#include "SoundFilter.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Failsafe.h"
#include "Storage.h"
#include "Output.h"
#include "WiFi.h"

static const char *TAG = "Sound";
static TaskHandle_t xHandle = nullptr;

static AudioFile *audio;
static i2s_chan_handle_t i2sHandle;
static esp_http_client_handle_t httpClient;
static std::string address, httpPayload;
static bool isOK = false;

static bool init()
{
    if (Storage::GetSensorState(Configuration::Sensors::Recording))
    {
        audio = new AudioFile(48000, 16, 1000, 10);
        if (audio == nullptr)
        {
            Failsafe::AddFailure(TAG, "Failed to allocate audio");
            return false;
        }

        address = Storage::GetAddress() + Backend::RecordingURL + std::to_string(Storage::GetDeviceId());
        ESP_LOGI(TAG, "Recording registering address: %s", address.c_str());

        esp_http_client_config_t http_config = {
            .url = address.c_str(),
            .cert_pem = Configuration::WiFi::ServerCrt,
            .method = HTTP_METHOD_POST,
            .max_redirection_count = INT_MAX,
        };

        httpClient = esp_http_client_init(&http_config);
        if (httpClient == nullptr)
        {
            Failsafe::AddFailure(TAG, "Failed to initialize sound network");
            return false;
        }
    }

    else
    {
        audio = new AudioFile(16000, 16, 125, 0);
        if (audio == nullptr)
        {
            Failsafe::AddFailure(TAG, "Failed to allocate audio");
            return false;
        }
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
        .dma_desc_num = uint32_t(Storage::GetSensorState(Configuration::Sensors::Recording) ? 12 : 32),
        .dma_frame_num = audio->Header.SampleRate / 8 / 16,
        .auto_clear = true,
    };

    esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &i2sHandle);
    if (err != ESP_OK)
    {
        Failsafe::AddFailure(TAG, "Failed to create new i2s channel");
        return false;
    }

    err = i2s_channel_init_std_mode(i2sHandle, &i2s_config);
    if (err != ESP_OK)
    {
        Failsafe::AddFailure(TAG, "Failed to initialize i2s");
        return false;
    }

    err = i2s_channel_enable(i2sHandle);
    if (err != ESP_OK)
    {
        Failsafe::AddFailure(TAG, "Failed to enable i2s channel");
        return false;
    }

    for (int i = 0; i < 4; i++)
        i2s_channel_read(i2sHandle, audio->Buffer, audio->BufferLength, nullptr, portMAX_DELAY);

    double loudness = Sound::ReadLoudness();
    if (loudness == 0)
    {
        Failsafe::AddFailure(TAG, "No mic detected.");
        return false;
    }

    isOK = true;

    return isOK;
}

static void vTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Initializing task");

    if (init())
    {
        if (Storage::GetSensorState(Configuration::Sensors::Recording))
        {
            for (;;)
            {
                Sound::UpdateRecording();
            }
        }

        else
        {
            for (;;)
            {
                Sound::Update();
            }
        }
    }

    vTaskDelete(nullptr);
}

void Sound::Init()
{
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 4, &xHandle);
}

void Sound::Update()
{
    UpdateLoudness();
}

void Sound::UpdateRecording()
{
    WiFi::WaitForConnection();

    if (UpdateLoudness())
    {
        ESP_LOGI(TAG, "Continuing recording - dB: %d - threshold: %ld", (int)m_Loudness.Current(), Storage::GetLoudnessThreshold());
        ESP_LOGI(TAG, "Post request to URL: %s - size: %ld", address.c_str(), audio->TotalLength);
        UNIT_TIMER("Post request");
        Helpers::PrintFreeHeap();

        esp_err_t err = esp_http_client_open(httpClient, audio->TotalLength);
        if (err != ESP_OK)
        {
            Failsafe::AddFailure(TAG, "Registering sound recording failed");
            return;
        }

        Output::Blink(Output::LedG, 250, true);
        RegisterRecordings();
        Output::SetContinuity(Output::LedG, false);
    }
}

bool Sound::IsOK()
{
    return isOK;
}

double Sound::ReadLoudness()
{
    i2s_channel_read(i2sHandle, audio->Buffer, audio->BufferLength, nullptr, portMAX_DELAY);
    double rms = SoundFilter::CalculateRMS((int16_t *)audio->Buffer, audio->BufferCount);
    double decibel = 20.0f * log10(rms / MicInfo::Amplitude) + MicInfo::RefDB + MicInfo::OffsetDB;

    // ESP_LOGI(TAG, "Loudness: %fdB", decibel);

    if (decibel > MicInfo::FloorDB && decibel < MicInfo::PeakDB)
    {
        return decibel;
    }

    return 0;
}

bool Sound::UpdateLoudness()
{
    double decibel = ReadLoudness();

    if (decibel != 0)
    {
        m_Loudness.Update(decibel + Sound::Constants::LoudnessOffset);

        if (m_Loudness.Current() > Storage::GetLoudnessThreshold())
        {
            return true;
        }

        return false;
    }

    isOK = false;
    Failsafe::AddFailureDelayed(TAG, "Loudness not valid - dB: " + std::to_string(decibel));

    return false;
}

void Sound::RegisterRecordings()
{
    int length = esp_http_client_write(httpClient, (char *)&audio->Header, sizeof(WaveHeader));
    if (length < 0)
    {
        Failsafe::AddFailure(TAG, "Writing wav header failed");
        esp_http_client_close(httpClient);
        return;
    }

    i2s_adc_data_scale(audio->Buffer, audio->Buffer, audio->BufferLength);

    length = esp_http_client_write(httpClient, (char *)audio->Buffer, audio->BufferLength);
    if (length < 0)
    {
        Failsafe::AddFailure(TAG, "Writing wav bytes failed");
        esp_http_client_close(httpClient);
        return;
    }

    else if (length < audio->BufferLength)
    {
        ESP_LOGE(TAG, "Writing partially complete - buffer length: %ld - transfered length: %d", audio->BufferLength, length);
        esp_http_client_close(httpClient);
        return;
    }

    for (size_t byte_count = audio->Header.DataLength - audio->BufferLength; byte_count > 0; byte_count -= audio->BufferLength)
    {
        i2s_channel_read(i2sHandle, audio->Buffer, audio->BufferLength, nullptr, portMAX_DELAY);
        i2s_adc_data_scale(audio->Buffer, audio->Buffer, audio->BufferLength);

        length = esp_http_client_write(httpClient, (char *)audio->Buffer, audio->BufferLength);
        if (length < 0)
        {
            Failsafe::AddFailure(TAG, "Writing wav bytes failed");
            esp_http_client_close(httpClient);
            return;
        }

        else if (length < audio->BufferLength)
        {
            ESP_LOGE(TAG, "Writing partially complete - buffer length: %ld - transfered length: %d", audio->BufferLength, length);
            esp_http_client_close(httpClient);
            return;
        }
    }

    length = esp_http_client_fetch_headers(httpClient);
    if (length < 0)
    {
        Failsafe::AddFailure(TAG, "Getting backend response failed");
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
