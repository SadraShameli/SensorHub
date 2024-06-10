#pragma once
#include <cmath>
#include <vector>

namespace Sound
{
    struct Coefficients
    {
        float B1;
        float B2;
        float A1;
        float A2;
    };

    struct DelayStates
    {
        float W0 = 0.0f;
        float W1 = 0.0f;
    };

    class SoundFilter
    {
    public:
        SoundFilter(const float gain, std::vector<Coefficients> &&coeffs) : m_Gain(gain), m_Coefficients(std::move(coeffs)), m_DelayStates(m_Coefficients.size()) {}

        template <typename T>
        double Filter(T *input, T *output, size_t size)
        {
            if (m_Coefficients.size() < 1)
            {
                return 0.0f;
            }

            T *source = input;
            for (size_t i = 0; i < (m_Coefficients.size() - 1); i++)
            {
                Filter(source, output, size, m_Coefficients[i], m_DelayStates[i]);
                source = output;
            }

            return FilterRMS(source, output, size, m_Coefficients.back(), m_DelayStates.back(), m_Gain);
        }

        template <typename T>
        static double CalculateRMS(T *input, size_t size)
        {
            double sum_sqr = 0.0f;
            size_t count = size;

            for (; size > 0; size--)
            {
                float f0 = *input++;
                sum_sqr += f0 * f0;
            }

            return std::sqrt(sum_sqr / count);
        }

        template <typename T>
        static void Filter(T *input, T *output, size_t size, const Coefficients &coeffs, DelayStates &delays)
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
        static double FilterRMS(T *input, T *output, size_t size, const Coefficients &coeffs, DelayStates &delays, float gain)
        {
            float f0 = coeffs.B1;
            float f1 = coeffs.B2;
            float f2 = coeffs.A1;
            float f3 = coeffs.A2;
            float f4 = delays.W0;
            float f5 = delays.W1;
            float f6 = gain;

            double sum_sqr = 0.0f;
            size_t count = size;

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

    void i2s_adc_data_scale(uint8_t *s_buff, uint8_t *d_buff, uint32_t len)
    {
        uint32_t j = 0;
        uint32_t dac_value = 0;

        for (int i = 0; i < len; i += 2)
        {
            dac_value = ((((uint16_t)(s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
            d_buff[j++] = 0;
            d_buff[j++] = dac_value * 256 / 2048;
        }
    }

    inline static SoundFilter DC_Blocker = {
        1.0,
        {
            {-1.0, 0.0, +0.9992, 0},
        },
    };

    inline static SoundFilter INMP441 = {
        1.00197834654696,
        {
            {-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091},
        },
    };

    inline static SoundFilter A_weighting = {
        0.169994948147430,
        {
            {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
            {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
            {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989},
        },
    };

    inline static SoundFilter C_weighting = {
        -0.491647169337140,
        {
            {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
            {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
            {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.035636575668043},
        },
    };
}