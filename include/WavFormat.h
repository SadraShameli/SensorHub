#pragma once

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
        AudioFile(uint32_t sampleRate, uint32_t sampleBitrate, uint32_t bufferTime, uint32_t duration) : Header(sampleRate, sampleBitrate, 1, duration)
        {
            BufferCount = sampleRate * bufferTime / 1000;
            BufferLength = BufferCount * 16 / 8;
            TotalLength = Header.DataLength + sizeof(WaveHeader);
            Buffer = new uint8_t[BufferLength]();
        }

        uint8_t *Buffer;
        WaveHeader Header;
        uint32_t BufferCount, BufferLength, TotalLength;
    };
}