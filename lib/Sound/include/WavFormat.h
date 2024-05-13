#pragma once
#include <cmath>
#include "Backend.h"

#ifdef UNIT_ENABLE_SOUND_RECORDING
#define SAMPLE_RATE 48000
#define RECORD_TIME 5

#elif defined(UNIT_ENABLE_SOUND_REGISTERING)
#define SAMPLE_RATE 16000
#define RECORD_TIME 1
#endif

class WaveHeader
{
public:
    WaveHeader(uint32_t sampleRate, uint16_t sampleBitrate, uint16_t channelCount, uint32_t duration)
    {
        SampleRate = sampleRate;
        BitsPerSample = sampleBitrate * channelCount;
        BytesPerSample = BitsPerSample * 8;
        BytesPerSecond = SampleRate * BitsPerSample;
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
    static const uint32_t SampleRate = SAMPLE_RATE;
    static const uint32_t SampleBitrate = 2;
    static const uint32_t RecordTime = RECORD_TIME;
    static const uint32_t BufferTime = 500;
    static const uint32_t BufferCount = SampleRate * BufferTime / 1000;
    static const uint32_t BufferLength = BufferCount * SampleBitrate;
    inline static const WaveHeader Header = WaveHeader(SampleRate, SampleBitrate, 1, RecordTime);
    inline static int16_t Buffer[BufferCount] = {0};
};

class MicInfo
{
public:
    static constexpr float Sensitivity = -26.0f;
    static constexpr float RefDB = 94.0f;
    static constexpr float OffsetDB = 3.0f;
    static constexpr float PeakDB = 94.0f;
    static constexpr float FloorDB = 29.0f;
    static constexpr double Amplitude = pow(10, MicInfo::Sensitivity / 20.0f) * ((1 << (AudioFile::SampleBitrate - 1)) - 1);
};
