#pragma once

struct wave_header_t
{
    uint8_t riff_tag[4] = {'R', 'I', 'F', 'F'};
    uint32_t file_length;
    uint8_t wave_tag[4] = {'W', 'A', 'V', 'E'};
    uint8_t fmt_tag[4] = {'f', 'm', 't', ' '};
    uint32_t chunk_size = 16;
    uint16_t format_tag = 1;
    uint16_t num_channels = 1;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t bytes_per_sample;
    uint16_t bits_per_sample;
    uint8_t data_tag[4] = {'d', 'a', 't', 'a'};
    uint32_t data_length;

    wave_header_t(uint32_t sample_rates, uint16_t sample_bits, uint32_t duration)
    {
        sample_rate = sample_rates;
        bits_per_sample = sample_bits;
        bytes_per_sample = sample_bits * num_channels / 8;
        bytes_per_second = sample_rates * bytes_per_sample;
        data_length = bytes_per_second * duration;
        file_length = data_length + 36;
    }
};

template <typename T, size_t sample_rate, size_t buffer_time>
struct wave_audio_t
{
    wave_header_t header;
    size_t total_size;
    size_t buffer_length;
    size_t buffer_count;
    T buffer[sample_rate * buffer_time] = {0};

    wave_audio_t(uint32_t duration) : header(sample_rate, sizeof(T) * 8, duration)
    {
        total_size = header.data_length + sizeof(wave_header_t);
        buffer_count = header.sample_rate;
        buffer_length = buffer_count;
    }
};