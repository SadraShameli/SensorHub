#pragma once
#include <cmath>
#include "esp_heap_caps.h"
#include "Configuration.h"
#include "Storage.h"

namespace Sound
{
    class WaveHeader
    {
    public:
        WaveHeader(uint32_t sampleRate, uint16_t sampleBitrate, uint16_t channelCount, uint32_t duration)
        {
            SampleRate = sampleRate;
            BitsPerSample = sampleBitrate * channelCount;
            BytesPerSample = BitsPerSample / 8;
            BytesPerSecond = SampleRate * BytesPerSample;
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

    class AudioFile
    {
    public:
        AudioFile(uint32_t sampleRate, uint16_t sampleBitrate, uint32_t bufferTime, uint32_t duration, uint16_t channelCount = 1) : Header(sampleRate, sampleBitrate, channelCount, duration)
        {
            BufferCount = sampleRate * bufferTime / 1000;
            BufferLength = BufferCount * 16 / 8;
            TotalLength = Header.DataLength + sizeof(WaveHeader);
            DMA_FrameNum = 4092 / (sampleBitrate * channelCount / 8);
            DMA_DescNum = std::max(std::ceil((float)bufferTime / (Storage::GetSensorState(Configuration::Sensor::Recording) ? 3 : 1) / ((float)DMA_FrameNum / sampleRate * 1000)), 3.0f);
            Buffer = (uint8_t *)heap_caps_aligned_calloc(sampleBitrate, 1, BufferLength, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            assert(Buffer);
        }

        uint8_t *Buffer;
        WaveHeader Header;
        uint32_t BufferCount, BufferLength, TotalLength;
        uint32_t DMA_DescNum = 0, DMA_FrameNum = 0;
    };
}