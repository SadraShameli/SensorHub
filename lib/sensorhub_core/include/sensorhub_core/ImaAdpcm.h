#pragma once

#include <cstddef>
#include <cstdint>

namespace sensorhub::core {

inline constexpr int kImaStepTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

inline constexpr int kImaIndexTable[16] = {
    -1,
    -1,
    -1,
    -1,
    2,
    4,
    6,
    8,
    -1,
    -1,
    -1,
    -1,
    2,
    4,
    6,
    8,
};

inline constexpr uint16_t kImaWavBlockAlign = 256;
inline constexpr uint16_t kImaWavSamplesPerBlock = 505;

class ImaAdpcmEncoder {
   public:
    ImaAdpcmEncoder() = default;

    void Reset() {
        m_predictor = 0;
        m_index = 0;
    }

    int16_t Predictor() const { return m_predictor; }

    int8_t Index() const { return m_index; }

    void SetState(int16_t predictor, int8_t index) {
        m_predictor = predictor;
        m_index = index;
    }

    uint8_t EncodeSample(int16_t sample) {
        int diff = sample - m_predictor;
        const int step = kImaStepTable[m_index];

        uint8_t code = 0;
        if (diff < 0) {
            code = 8;
            diff = -diff;
        }

        int tmp = step;
        if (diff >= tmp) {
            code |= 4;
            diff -= tmp;
        }
        tmp >>= 1;
        if (diff >= tmp) {
            code |= 2;
            diff -= tmp;
        }
        tmp >>= 1;
        if (diff >= tmp) {
            code |= 1;
        }

        int diffq = step >> 3;
        if (code & 4)
            diffq += step;
        if (code & 2)
            diffq += step >> 1;
        if (code & 1)
            diffq += step >> 2;
        if (code & 8)
            diffq = -diffq;

        int new_pred = m_predictor + diffq;
        if (new_pred > 32767)
            new_pred = 32767;
        if (new_pred < -32768)
            new_pred = -32768;
        m_predictor = static_cast<int16_t>(new_pred);

        int new_index = m_index + kImaIndexTable[code];
        if (new_index < 0)
            new_index = 0;
        if (new_index > 88)
            new_index = 88;
        m_index = static_cast<int8_t>(new_index);

        return code;
    }

    bool EncodeWavBlock(const int16_t* samples, std::size_t sample_count,
                        uint8_t* out) {
        if (sample_count != kImaWavSamplesPerBlock) {
            return false;
        }

        m_predictor = samples[0];

        out[0] = static_cast<uint8_t>(m_predictor & 0xFF);
        out[1] = static_cast<uint8_t>((m_predictor >> 8) & 0xFF);
        out[2] = static_cast<uint8_t>(m_index);
        out[3] = 0;

        std::size_t out_byte = 4;
        bool high_nibble = false;
        for (std::size_t i = 1; i < sample_count; ++i) {
            const uint8_t code = EncodeSample(samples[i]);
            if (!high_nibble) {
                out[out_byte] = code & 0x0F;
                high_nibble = true;
            } else {
                out[out_byte] |= static_cast<uint8_t>((code & 0x0F) << 4);
                ++out_byte;
                high_nibble = false;
            }
        }

        return true;
    }

   private:
    int16_t m_predictor = 0;
    int8_t m_index = 0;
};

class ImaAdpcmDecoder {
   public:
    ImaAdpcmDecoder() = default;

    void SetState(int16_t predictor, int8_t index) {
        m_predictor = predictor;
        m_index = index;
    }

    int16_t DecodeSample(uint8_t code) {
        const int step = kImaStepTable[m_index];

        int diffq = step >> 3;
        if (code & 4)
            diffq += step;
        if (code & 2)
            diffq += step >> 1;
        if (code & 1)
            diffq += step >> 2;
        if (code & 8)
            diffq = -diffq;

        int new_pred = m_predictor + diffq;
        if (new_pred > 32767)
            new_pred = 32767;
        if (new_pred < -32768)
            new_pred = -32768;
        m_predictor = static_cast<int16_t>(new_pred);

        int new_index = m_index + kImaIndexTable[code & 0x0F];
        if (new_index < 0)
            new_index = 0;
        if (new_index > 88)
            new_index = 88;
        m_index = static_cast<int8_t>(new_index);

        return m_predictor;
    }

   private:
    int16_t m_predictor = 0;
    int8_t m_index = 0;
};

}
