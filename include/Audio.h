#pragma once

#include <cmath>
#include <cstdlib>
#include <memory>

#include "Storage.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace Mic {

struct WavHeader {
    WavHeader(uint32_t sampleRate, uint16_t sampleBitrate,
              uint16_t channelCount, uint32_t duration)
        : SampleRate(sampleRate) {
        BitsPerSample = sampleBitrate;
        ChannelCount = channelCount;
        BytesPerSample = (sampleBitrate * channelCount) / 8;
        BytesPerSecond = sampleRate * BytesPerSample;
        DataLength = BytesPerSecond * duration;
        FileLength = DataLength + 36;
    }

    uint8_t RiffTag[4] = {'R', 'I', 'F', 'F'};
    uint32_t FileLength;
    uint8_t WaveTag[4] = {'W', 'A', 'V', 'E'};
    uint8_t FmtTag[4] = {'f', 'm', 't', ' '};
    uint32_t ChunkSize = 16;
    uint16_t FormatTag = 1;
    uint16_t ChannelCount = 1;
    uint32_t SampleRate;
    uint32_t BytesPerSecond;
    uint16_t BytesPerSample;
    uint16_t BitsPerSample;
    uint8_t DataTag[4] = {'d', 'a', 't', 'a'};
    uint32_t DataLength;
};

struct HeapCapsDeleter {
    void operator()(void* p) const { heap_caps_free(p); }
};

using DmaBuffer = std::unique_ptr<uint8_t[], HeapCapsDeleter>;

struct Audio {
    Audio(uint32_t sampleRate, uint16_t sampleBitrate, uint32_t bufferTime,
          uint32_t duration, uint16_t channelCount = 1)
        : Header(sampleRate, sampleBitrate, channelCount, duration),
          BufferCount(sampleRate * bufferTime / 1000) {

        BufferLength = BufferCount * sampleBitrate / 8;
        TotalLength = Header.DataLength + sizeof(WavHeader);
        DMA_FrameNum = static_cast<uint32_t>(
            4092.0f / (sampleBitrate * channelCount / 8.0f));
        if (DMA_FrameNum < 8) {
            DMA_FrameNum = 8;
        }
        DMA_DescNum = static_cast<uint32_t>(std::max(
            std::ceil(static_cast<float>(bufferTime) /
                      (Storage::GetSensorState(Configuration::Sensor::Recording)
                           ? 3.0f
                           : 1.0f) /
                      (static_cast<float>(DMA_FrameNum) /
                       static_cast<float>(sampleRate) * 1000.0f)),
            3.0f));

        void* mem =
            heap_caps_malloc(BufferLength, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (mem == nullptr) {
            ESP_LOGE("Audio",
                     "DMA buffer alloc failed (%u bytes)",
                     (unsigned)BufferLength);
            std::abort();
        }
        Buffer = DmaBuffer(static_cast<uint8_t*>(mem));
    }

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    Audio(Audio&&) = delete;
    Audio& operator=(Audio&&) = delete;

    DmaBuffer Buffer;
    WavHeader Header;
    uint32_t BufferCount, BufferLength, TotalLength, DMA_DescNum, DMA_FrameNum;
};

template <typename T>
static float CalculateRMS(T* input, uint32_t size) {
    if (size == 0) {
        return 0.0f;
    }
    double sum_sqr = 0.0;

    for (uint32_t count = size; count > 0; count--) {
        const double f0 = static_cast<double>(*input++);
        sum_sqr += f0 * f0;
    }

    return static_cast<float>(std::sqrt(sum_sqr / static_cast<double>(size)));
}

}