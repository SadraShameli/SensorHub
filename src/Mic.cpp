#include "Mic.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "AdpcmRecorder.h"
#include "Audio.h"
#include "Backend.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Display.h"
#include "Failsafe.h"
#include "Output.h"
#include "Storage.h"
#include "WiFi.h"
#include "driver/i2s_std.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "sensors/ISensor.h"
#include "sensors/SensorRegistry.h"

namespace Mic {
namespace Constants {

static const float Sensitivity = 26.0f, RefDB = 94.0f, OffsetDB = -3.0f,
                   PeakDB = 116.0f, FloorDB = 29.0f;

static const float Amplitude =
    powf(10.f, -Sensitivity / 20.0f) * ((1 << (16 - 1)) - 1);

static const float LoudnessOffset = 0;

}

static const char* TAG = "Sound";
static const char* TAG_SENDER = "SoundSender";
static TaskHandle_t xHandle = nullptr;
static TaskHandle_t xSenderHandle = nullptr;

static i2s_chan_handle_t i2sHandle = nullptr;

static std::unique_ptr<Audio> audio;
static std::unique_ptr<AdpcmEncoderState> encoder;
static std::unique_ptr<AdpcmRing> ring;
static Reading loudness;
static bool isOK = false;
static bool recordingMode = false;

static std::atomic<uint32_t> s_preRollAtTrigger{0};

alignas(uint32_t) static uint8_t
    s_ringStorage[AdpcmConfig::RingCapacityBlocks * AdpcmConfig::BlockAlign];

namespace {

class LoudnessSensor : public Sensors::ISensor {
   public:
    const char* Name() const override { return "Loudness"; }

    const char* Unit() const override { return "dB"; }

    uint8_t Id() const override { return Configuration::Sensor::Loudness; }

    bool IsOk() const override { return isOK; }

    Reading Snapshot() const override {
        return Reading(loudness.Current(), loudness.Min(), loudness.Max());
    }

    void ResetMinMax() override { loudness.Reset(); }

    int ReportingValue() const override {
        return static_cast<int>(loudness.Max());
    }
};

LoudnessSensor s_loudness;

}

static std::string address, httpPayload;
static uint32_t transferLength = 0, transferCount = 0;

static float ComputeDbFromPcm(const int16_t* samples, uint32_t count) {
    const float rms = CalculateRMS(samples, count);
    return 20.0f * log10f(rms / Constants::Amplitude) + Constants::RefDB +
           Constants::OffsetDB;
}

static void UpdateLoudnessFromPcm(const int16_t* samples, uint32_t count) {
    const float decibel = ComputeDbFromPcm(samples, count);
    if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
        loudness.Update(decibel + Constants::LoudnessOffset);
        isOK = true;
    }
}

static void CaptureRecordingIteration() {
    int16_t* pcm = encoder->PcmBuffer();
    if (i2s_channel_read(i2sHandle,
                         pcm,
                         encoder->PcmBufferBytes(),
                         nullptr,
                         portMAX_DELAY) != ESP_OK) {
        return;
    }

    UpdateLoudnessFromPcm(pcm, AdpcmConfig::SamplesPerBlock);

    const AdpcmRing::State state = ring->CurrentState();

    if (state == AdpcmRing::State::PostCaptureDrain) {
        return;
    }

    uint8_t block[AdpcmConfig::BlockAlign];
    if (!encoder->Encode(block)) {
        return;
    }

    if (state == AdpcmRing::State::Idle) {
        ring->PushOverwrite(block);

        if (!WiFi::IsConnected()) {
            return;
        }
        if ((uint32_t)loudness.Current() <= Storage::GetLoudnessThreshold()) {
            return;
        }

        const uint32_t preRoll = ring->BeginRecording();
        if (preRoll == 0) {
            return;
        }

        s_preRollAtTrigger.store(preRoll, std::memory_order_release);

        ESP_LOGI(TAG,
                 "Loud event %d dB (threshold %ld) - preroll=%lu blocks",
                 (int)loudness.Current(),
                 Storage::GetLoudnessThreshold(),
                 (unsigned long)preRoll);

        if (xSenderHandle != nullptr) {
            xTaskNotifyGive(xSenderHandle);
        }
    } else {
        ring->PushPreserve(block);
        if (xSenderHandle != nullptr) {
            xTaskNotifyGive(xSenderHandle);
        }

        if (ring->PostRollProduced() >= AdpcmConfig::PostRollBlocks) {
            ring->MarkPostRollDone();
        }
    }
}

static void vTask(void* arg) {
    ESP_LOGI(TAG, "Initializing");

    recordingMode = Storage::GetSensorState(Configuration::Sensor::Recording);
    uint32_t sampleRate = 0;

    if (recordingMode) {
        encoder = std::make_unique<AdpcmEncoderState>();
        ring = std::make_unique<AdpcmRing>(s_ringStorage);
        sampleRate = AdpcmConfig::SampleRateHz;
    } else {
        audio = std::make_unique<Audio>(16000, 16, 125, 0);
        transferLength = audio->BufferLength;
        transferCount = audio->BufferCount;
        sampleRate = audio->Header.SampleRate;
    }

    const uint32_t dmaFrameNum = recordingMode ? 256 : audio->DMA_FrameNum;
    const uint32_t dmaDescNum = recordingMode ? 4 : audio->DMA_DescNum;

    const i2s_std_config_t i2s_config = {
        .clk_cfg =
            {
                .sample_rate_hz = sampleRate,
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
        .dma_desc_num = dmaDescNum,
        .dma_frame_num = dmaFrameNum,
        .auto_clear = true,
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &i2sHandle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2sHandle, &i2s_config));
    ESP_ERROR_CHECK(i2s_channel_enable(i2sHandle));

    if (recordingMode) {
        int16_t* pcm = encoder->PcmBuffer();
        for (int i = 0; i < 4; i++) {
            ESP_ERROR_CHECK(i2s_channel_read(i2sHandle,
                                             pcm,
                                             encoder->PcmBufferBytes(),
                                             nullptr,
                                             portMAX_DELAY));
        }

        int32_t peakAbs = 0;
        for (uint32_t i = 0; i < AdpcmConfig::SamplesPerBlock; i++) {
            const int32_t v = pcm[i] < 0 ? -pcm[i] : pcm[i];
            if (v > peakAbs)
                peakAbs = v;
        }

        if (peakAbs < 4) {
            ESP_LOGW(TAG, "No mic detected (peak=%d), skipping", (int)peakAbs);
            vTaskDelete(nullptr);
        }

        isOK = true;
        UpdateLoudnessFromPcm(pcm, AdpcmConfig::SamplesPerBlock);

        for (;;) {
            CaptureRecordingIteration();
        }
    }

    for (int i = 0; i < 6; i++) {
        ESP_ERROR_CHECK(i2s_channel_read(i2sHandle,
                                         audio->Buffer.get(),
                                         transferLength,
                                         nullptr,
                                         portMAX_DELAY));
    }

    {
        const float decibel =
            ComputeDbFromPcm((int16_t*)audio->Buffer.get(), transferCount);
        if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
            loudness.Update(decibel + Constants::LoudnessOffset);
            isOK = true;
        } else {
            ESP_LOGW(TAG, "No mic detected, skipping");
            vTaskDelete(nullptr);
        }
    }

    for (;;) {
        Update();
    }

    vTaskDelete(nullptr);
}

static bool ReadHttpResponse(esp_http_client_handle_t client,
                             int& outStatusCode) {
    int length = (int)esp_http_client_fetch_headers(client);
    outStatusCode = esp_http_client_get_status_code(client);

    if (esp_http_client_is_chunked_response(client)) {
        httpPayload.clear();
        httpPayload.resize(DEFAULT_HTTP_BUF_SIZE);
    } else if (length > 0) {
        httpPayload.clear();
        httpPayload.resize(length);
    } else {
        return false;
    }

    esp_http_client_read(client, httpPayload.data(), httpPayload.capacity());
    return true;
}

static void SenderTask(void* arg) {
    ESP_LOGI(TAG_SENDER, "Initializing");

    address = Storage::GetAddress() + Backend::RecordingURL +
              std::to_string(Storage::GetDeviceId());
    ESP_LOGI(TAG_SENDER, "Recording register address: %s", address.c_str());

    esp_http_client_config_t httpConfig = {
        .url = address.c_str(),
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .max_redirection_count = INT_MAX,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t httpClient = esp_http_client_init(&httpConfig);
    assert(httpClient);

    const std::string authBearer = "Bearer " + Storage::GetAuthKey();
    esp_http_client_set_header(httpClient, "Authorization", authBearer.c_str());

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ring == nullptr) {
            continue;
        }

        const AdpcmRing::State state = ring->CurrentState();
        if (state == AdpcmRing::State::Idle) {
            continue;
        }

        const uint32_t preRoll =
            s_preRollAtTrigger.load(std::memory_order_acquire);
        const uint32_t total = preRoll + AdpcmConfig::PostRollBlocks;
        const uint32_t totalBytes =
            sizeof(WavHeaderImaAdpcm) + total * AdpcmConfig::BlockAlign;

        ESP_LOGI(
            TAG_SENDER,
            "Recording start - preroll=%lu post=%lu total=%lu blocks (%lu B)",
            (unsigned long)preRoll,
            (unsigned long)AdpcmConfig::PostRollBlocks,
            (unsigned long)total,
            (unsigned long)totalBytes);

        UNIT_TIMER("POST request");

        esp_err_t err = esp_http_client_open(httpClient, totalBytes);
        if (err != ESP_OK) {
            Failsafe::AddFailure(
                TAG_SENDER,
                "POST open failed - " + (err == ESP_ERR_HTTP_CONNECT
                                             ? "URL not found: " + address
                                             : esp_err_to_name(err)));
            ring->EndRecording();
            continue;
        }

        Output::Blink(Output::LedG, 250, true);

        WavHeaderImaAdpcm header(AdpcmConfig::SampleRateHz, total);
        int written = esp_http_client_write(httpClient,
                                            (const char*)&header,
                                            sizeof(header));
        if (written < 0) {
            Failsafe::AddFailure(TAG_SENDER, "Writing WAV header failed");
            esp_http_client_close(httpClient);
            Output::SetContinuity(Output::LedG, false);
            ring->EndRecording();
            continue;
        }

        uint32_t sent = 0;
        bool failed = false;
        uint8_t block[AdpcmConfig::BlockAlign];

        while (sent < total) {
            if (ring->Pop(block)) {
                int n = esp_http_client_write(httpClient,
                                              (const char*)block,
                                              AdpcmConfig::BlockAlign);
                if (n != AdpcmConfig::BlockAlign) {
                    Failsafe::AddFailure(TAG_SENDER, "HTTP write failed");
                    failed = true;
                    break;
                }
                sent++;
            } else {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
            }
        }

        if (failed) {
            esp_http_client_close(httpClient);
            Output::SetContinuity(Output::LedG, false);
            ring->EndRecording();
            continue;
        }

        int statusCode = 0;
        const bool responseOk = ReadHttpResponse(httpClient, statusCode);
        esp_http_client_close(httpClient);

        if (!responseOk) {
            Failsafe::AddFailure(
                TAG_SENDER,
                "Status: " + std::to_string(statusCode) + " - empty response");
        } else if (!Backend::CheckResponseFailed(
                       httpPayload,
                       (HTTP::Status::StatusCode)statusCode)) {
            ResetValues();
        }

        const uint32_t dropped = ring->DroppedDuringRecording();
        if (dropped > 0) {
            ESP_LOGW(TAG_SENDER,
                     "Recording done - sent=%lu blocks status=%d (dropped=%lu)",
                     (unsigned long)sent,
                     statusCode,
                     (unsigned long)dropped);
        } else {
            ESP_LOGI(TAG_SENDER,
                     "Recording done - sent=%lu blocks status=%d",
                     (unsigned long)sent,
                     statusCode);
        }

        Output::SetContinuity(Output::LedG, false);
        ring->EndRecording();
    }
}

static void RegisterSensors() {
    Sensors::SensorRegistry::Instance().Register(&s_loudness);
}

static bool ShouldStart() {
    using S = Configuration::Sensor::Sensors;
    return Storage::GetSensorState(S::Loudness) ||
           Storage::GetSensorState(S::Recording);
}

static bool ShouldStartSender() {
    return Storage::GetSensorState(Configuration::Sensor::Recording);
}

void Init() {
    RegisterSensors();
    xTaskCreate(&vTask, TAG, 8192, nullptr, tskIDLE_PRIORITY + 4, &xHandle);
}

const Kernel::Service kService = {
    .name = "Mic",
    .modes = Kernel::RunInNormalMode,
    .on_init = &RegisterSensors,
    .task_entry = &vTask,
    .stack_bytes = 8192,
    .priority = tskIDLE_PRIORITY + 4,
    .out_handle = &xHandle,
    .should_start = &ShouldStart,
};

const Kernel::Service kSenderService = {
    .name = "MicSender",
    .modes = Kernel::RunInNormalMode,
    .on_init = nullptr,
    .task_entry = &SenderTask,
    .stack_bytes = 8192,
    .priority = tskIDLE_PRIORITY + 3,
    .out_handle = &xSenderHandle,
    .should_start = &ShouldStartSender,
};

void Update() {
    UpdateLoudness();
}

void UpdateRecording() {
    CaptureRecordingIteration();
}

bool UpdateLoudness() {
    i2s_channel_read(i2sHandle,
                     audio->Buffer.get(),
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

float CalculateLoudness() {
    float rms = CalculateRMS((int16_t*)audio->Buffer.get(), audio->BufferCount);
    float decibel = 20.0f * log10f(rms / Constants::Amplitude) +
                    Constants::RefDB + Constants::OffsetDB;

    ESP_LOGD(TAG, "Loudness: %ddB", (int)decibel);

    if (decibel > Constants::FloorDB && decibel < Constants::PeakDB) {
        return decibel;
    }

    return 0;
}

bool IsOK() {
    return isOK;
}

void ResetValues() {
    loudness.Reset();
}

const Reading& GetLoudness() {
    return loudness;
}

}
