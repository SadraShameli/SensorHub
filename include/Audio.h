#pragma once

#include <cmath>
#include <vector>

#include "esp_heap_caps.h"

#include "Storage.h"

namespace Mic
{
    struct Coefficients
    {
        float B1 = 0.0f;
        float B2 = 0.0f;
        float A1 = 0.0f;
        float A2 = 0.0f;
    };

    struct DelayStates
    {
        float W0 = 0.0f;
        float W1 = 0.0f;
    };

    class AudioFilter
    {
    public:
        AudioFilter(const float gain, std::vector<Coefficients> &&coeffs) : m_Gain(gain), m_Coefficients(std::move(coeffs)), m_DelayStates(m_Coefficients.size()) {}

        template <typename T>
        float Filter(T *input, T *output, uint32_t size)
        {
            if (m_Coefficients.size() < 1)
                return 0.0f;

            T *source = input;
            for (uint32_t i = 0; i < (m_Coefficients.size() - 1); i++)
            {
                Filter(source, output, size, m_Coefficients[i], m_DelayStates[i]);
                source = output;
            }

            return FilterRMS(source, output, size, m_Coefficients.back(), m_DelayStates.back(), m_Gain);
        }

        template <typename T>
        void Filter(T *input, T *output, uint32_t size, const Coefficients &coeffs, DelayStates &delays)
        {
            float f0 = coeffs.B1;
            float f1 = coeffs.B2;
            float f2 = coeffs.A1;
            float f3 = coeffs.A2;
            float f4 = delays.W0;
            float f5 = delays.W1;

            for (; size > 0; size--)
            {
                float f6 = *input++;
                f6 += f2 * f4;
                f6 += f3 * f5;
                float f7 = f6;
                f7 += f0 * f4;
                f7 += f1 * f5;
                *output++ = (T)f7;
                f5 = f4;
                f4 = f6;
            }

            delays.W0 = f4;
            delays.W1 = f5;
        }

        template <typename T>
        float FilterRMS(T *input, T *output, uint32_t size, const Coefficients &coeffs, DelayStates &delays, float gain)
        {
            float f0 = coeffs.B1;
            float f1 = coeffs.B2;
            float f2 = coeffs.A1;
            float f3 = coeffs.A2;
            float f4 = delays.W0;
            float f5 = delays.W1;
            float f6 = gain;

            float sum_sqr = 0.0f;
            uint32_t count = size;

            for (; size > 0; size--)
            {
                float f7 = *input++;
                f7 += f2 * f4;
                f7 += f3 * f5;
                float f8 = f7;
                float f10 = f6;
                f8 += f0 * f4;
                f8 += f1 * f5;
                float f9 = f8 * f6;
                *output++ = (T)f9;
                f5 = f4;
                f4 = f10;
                sum_sqr += f9 * f9;
            }

            delays.W0 = f4;
            delays.W1 = f5;

            return sqrt(sum_sqr / count);
        }

    private:
        float m_Gain;

        std::vector<Coefficients> m_Coefficients;
        std::vector<DelayStates> m_DelayStates;
    };

    struct WavHeader
    {
        WavHeader(uint32_t sampleRate, uint16_t sampleBitrate, uint16_t channelCount, uint32_t duration)
            : SampleRate(sampleRate), BitsPerSample(sampleBitrate * channelCount)
        {
            BytesPerSample = BitsPerSample / 8;
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

    struct Audio
    {
        Audio(uint32_t sampleRate, uint16_t sampleBitrate, uint32_t bufferTime, uint32_t duration, uint16_t channelCount = 1)
            : Header(sampleRate, sampleBitrate, channelCount, duration), BufferCount(sampleRate * bufferTime / 1000)
        {
            BufferLength = BufferCount * 16 / 8;
            TotalLength = Header.DataLength + sizeof(WavHeader);
            DMA_FrameNum = (uint32_t)(4092.0f / (sampleBitrate * channelCount / 8.0f));
            DMA_DescNum = (uint32_t)std::max(std::ceil((float)bufferTime / (Storage::GetSensorState(Configuration::Sensor::Recording) ? 3.0f : 1.0f) / ((float)DMA_FrameNum / (float)sampleRate * 1000.0f)), 3.0f);
            Buffer = new uint8_t[BufferLength];
        }

        uint8_t *Buffer;
        WavHeader Header;
        uint32_t BufferCount, BufferLength, TotalLength, DMA_DescNum, DMA_FrameNum;
    };

    inline void NormalizeAudio(int16_t *samples, uint32_t count)
    {
        int16_t max = 0;
        for (uint32_t i = 0; i < count; i++)
            if (std::abs(samples[i]) > max)
                max = samples[i];

        float scaleFactor = 32767.0f / max;
        for (uint32_t i = 0; i < count; i++)
            samples[i] = (int16_t)(samples[i] * scaleFactor);
    }

    template <typename T>
    static float CalculateRMS(T *input, uint32_t size)
    {
        float sum_sqr = 0.0f;

        for (uint32_t count = size; count > 0; count--)
        {
            float f0 = *input++;
            sum_sqr += f0 * f0;
        }

        return std::sqrt(sum_sqr / (float)size);
    }

    // inline AudioFilter DC_Blocker = {
    //     1.0,
    //     {
    //         {-1.0, 0.0, +0.9992, 0},
    //     },
    // };

    // inline AudioFilter INMP441 = {
    //     1.00197834654696,
    //     {
    //         {-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091},
    //     },
    // };

    // inline AudioFilter A_weighting = {
    //     0.169994948147430,
    //     {
    //         {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
    //         {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
    //         {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989},
    //     },
    // };

    // inline AudioFilter C_weighting = {
    //     -0.491647169337140,
    //     {
    //         {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
    //         {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
    //         {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.035636575668043},
    //     },
    // };
}