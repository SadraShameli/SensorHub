#include <unity.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "sensorhub_core/Altitude.h"
#include "sensorhub_core/ImaAdpcm.h"
#include "sensorhub_core/LoudnessMath.h"
#include "sensorhub_core/MapValue.h"
#include "sensorhub_core/Reading.h"
#include "sensorhub_core/Rms.h"
#include "sensorhub_core/Strings.h"
#include "sensorhub_core/UrlValidator.h"

using namespace sensorhub::core;

void setUp() {}

void tearDown() {}

void test_reading_default_state_accepts_negative_values() {
    Reading r;
    r.Update(-50.0f);
    TEST_ASSERT_EQUAL_FLOAT(-50.0f, r.Current());
    TEST_ASSERT_EQUAL_FLOAT(-50.0f, r.Min());
    TEST_ASSERT_EQUAL_FLOAT(-50.0f, r.Max());
}

void test_reading_tracks_min_and_max() {
    Reading r;
    r.Update(10.0f);
    r.Update(5.0f);
    r.Update(30.0f);
    r.Update(-2.0f);
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, r.Current());
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, r.Min());
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r.Max());
}

void test_reading_reset_collapses_to_current() {
    Reading r;
    r.Update(10.0f);
    r.Update(20.0f);
    r.Reset();
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.Current());
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.Min());
    TEST_ASSERT_EQUAL_FLOAT(20.0f, r.Max());
}

void test_mapvalue_linear() {
    TEST_ASSERT_EQUAL_INT64(50, MapValue<long>(5, 0, 10, 0, 100));
    TEST_ASSERT_EQUAL_INT64(0, MapValue<long>(0, 0, 10, 0, 100));
    TEST_ASSERT_EQUAL_INT64(100, MapValue<long>(10, 0, 10, 0, 100));
}

void test_mapvalue_zero_range_returns_sentinel() {
    TEST_ASSERT_EQUAL_INT64(-1, MapValue<long>(5, 10, 10, 0, 100));
}

void test_remove_trailing_whitespace_basic() {
    std::string s = "hello   \r\n";
    RemoveTrailingWhitespace(s);
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
}

void test_remove_trailing_whitespace_all_blank() {
    std::string s = "   \t\n";
    RemoveTrailingWhitespace(s);
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
}

void test_remove_trailing_whitespace_empty() {
    std::string s;
    RemoveTrailingWhitespace(s);
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
}

void test_altitude_at_sea_level_is_zero() {
    const float alt = CalculateAltitude(1013.25f, 1013.25f, 15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, alt);
}

void test_altitude_decreases_with_lower_pressure() {
    const float low = CalculateAltitude(800.0f, 1013.25f, 15.0f);
    const float high = CalculateAltitude(1013.25f, 1013.25f, 15.0f);
    TEST_ASSERT_TRUE(low > high);
    TEST_ASSERT_TRUE(low > 1000.0f);
    TEST_ASSERT_TRUE(low < 3000.0f);
}

void test_rms_of_zeros_is_zero() {
    int16_t buf[8] = {0};
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateRms(buf, 8));
}

void test_rms_of_empty_buffer_is_zero() {
    int16_t buf[1] = {0};
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateRms(buf, 0));
}

void test_rms_of_constant_signal() {
    int16_t buf[4] = {100, 100, 100, 100};
    TEST_ASSERT_EQUAL_FLOAT(100.0f, CalculateRms(buf, 4));
}

void test_rms_of_alternating_signal() {
    int16_t buf[4] = {100, -100, 100, -100};
    TEST_ASSERT_EQUAL_FLOAT(100.0f, CalculateRms(buf, 4));
}

void test_loudness_zero_rms_returns_zero() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, RmsToSpl(0.0f, FullScaleAmplitude16Bit()));
}

void test_loudness_below_floor_returns_zero() {

    TEST_ASSERT_EQUAL_FLOAT(0.0f, RmsToSpl(0.001f, FullScaleAmplitude16Bit()));
}

void test_loudness_in_range_returns_positive_db() {
    const float amp = FullScaleAmplitude16Bit();
    const float spl = RmsToSpl(amp * 0.1f, amp);
    TEST_ASSERT_TRUE(spl > kInmp441FloorDb);
    TEST_ASSERT_TRUE(spl < kInmp441PeakDb);
}

void test_url_accepts_well_formed_https() {
    TEST_ASSERT_TRUE(IsAllowedBackendUrl("https://sadra.nl/api/"));
    TEST_ASSERT_TRUE(IsAllowedBackendUrl("https://example.com"));
    TEST_ASSERT_TRUE(
        IsAllowedBackendUrl("https://api.example.com/v1/devices/"));
}

void test_url_rejects_http_by_default() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("http://example.com"));
}

void test_url_rejects_other_schemes() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("file:///etc/passwd"));
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("ftp://example.com"));
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("ws://example.com"));
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("javascript:alert(1)"));
}

void test_url_rejects_userinfo() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https://user:pass@example.com"));
}

void test_url_rejects_ip_literal_by_default() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https://192.168.1.1/"));
}

void test_url_accepts_ip_literal_when_allowed() {
    UrlValidatorOptions opts;
    opts.allow_ip_literal = true;
    opts.require_dot_in_host = false;
    TEST_ASSERT_TRUE(IsAllowedBackendUrl("https://192.168.1.1/", opts));
}

void test_url_rejects_no_host() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https://"));
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https:///path"));
}

void test_url_rejects_whitespace() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https:// example.com"));
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https://example .com"));
}

void test_url_rejects_oversized() {
    std::string big = "https://";
    big.append(240, 'a');
    big += ".com";
    TEST_ASSERT_FALSE(IsAllowedBackendUrl(big));
}

void test_url_rejects_bare_localhost() {
    TEST_ASSERT_FALSE(IsAllowedBackendUrl("https://localhost/"));
}

void test_adpcm_round_trip_silence() {
    ImaAdpcmEncoder enc;
    int16_t samples[kImaWavSamplesPerBlock] = {0};
    uint8_t block[kImaWavBlockAlign] = {0};
    TEST_ASSERT_TRUE(
        enc.EncodeWavBlock(samples, kImaWavSamplesPerBlock, block));

    ImaAdpcmDecoder dec;
    int16_t first = static_cast<int16_t>(block[0] | (block[1] << 8));
    int8_t idx = static_cast<int8_t>(block[2]);
    TEST_ASSERT_EQUAL_INT16(0, first);
    TEST_ASSERT_EQUAL_INT8(0, idx);
    dec.SetState(first, idx);

    for (std::size_t i = 4; i < kImaWavBlockAlign; ++i) {
        const uint8_t lo = block[i] & 0x0F;
        const uint8_t hi = (block[i] >> 4) & 0x0F;
        const int16_t s1 = dec.DecodeSample(lo);
        const int16_t s2 = dec.DecodeSample(hi);
        TEST_ASSERT_INT16_WITHIN(8, 0, s1);
        TEST_ASSERT_INT16_WITHIN(8, 0, s2);
    }
}

void test_adpcm_round_trip_ramp_is_close() {
    ImaAdpcmEncoder enc;
    int16_t samples[kImaWavSamplesPerBlock];
    for (std::size_t i = 0; i < kImaWavSamplesPerBlock; ++i) {
        samples[i] = static_cast<int16_t>(i * 32);
    }

    uint8_t block[kImaWavBlockAlign] = {0};
    TEST_ASSERT_TRUE(
        enc.EncodeWavBlock(samples, kImaWavSamplesPerBlock, block));

    ImaAdpcmDecoder dec;
    int16_t pred = static_cast<int16_t>(block[0] | (block[1] << 8));
    int8_t idx = static_cast<int8_t>(block[2]);
    TEST_ASSERT_EQUAL_INT16(samples[0], pred);
    dec.SetState(pred, idx);

    int16_t decoded[kImaWavSamplesPerBlock];
    decoded[0] = pred;
    std::size_t out = 1;
    for (std::size_t i = 4;
         i < kImaWavBlockAlign && out < kImaWavSamplesPerBlock;
         ++i) {
        decoded[out++] = dec.DecodeSample(block[i] & 0x0F);
        if (out >= kImaWavSamplesPerBlock)
            break;
        decoded[out++] = dec.DecodeSample((block[i] >> 4) & 0x0F);
    }

    double sum_err_sq = 0.0;
    double sum_sig_sq = 0.0;
    for (std::size_t i = 0; i < kImaWavSamplesPerBlock; ++i) {
        const double err =
            static_cast<double>(samples[i]) - static_cast<double>(decoded[i]);
        sum_err_sq += err * err;
        sum_sig_sq += static_cast<double>(samples[i]) * samples[i];
    }
    const double snr = 10.0 * std::log10(sum_sig_sq / sum_err_sq);
    TEST_ASSERT_TRUE(snr > 30.0);
}

void test_adpcm_block_rejects_wrong_size() {
    ImaAdpcmEncoder enc;
    int16_t samples[10] = {0};
    uint8_t block[kImaWavBlockAlign] = {0};
    TEST_ASSERT_FALSE(enc.EncodeWavBlock(samples, 10, block));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_reading_default_state_accepts_negative_values);
    RUN_TEST(test_reading_tracks_min_and_max);
    RUN_TEST(test_reading_reset_collapses_to_current);

    RUN_TEST(test_mapvalue_linear);
    RUN_TEST(test_mapvalue_zero_range_returns_sentinel);

    RUN_TEST(test_remove_trailing_whitespace_basic);
    RUN_TEST(test_remove_trailing_whitespace_all_blank);
    RUN_TEST(test_remove_trailing_whitespace_empty);

    RUN_TEST(test_altitude_at_sea_level_is_zero);
    RUN_TEST(test_altitude_decreases_with_lower_pressure);

    RUN_TEST(test_rms_of_zeros_is_zero);
    RUN_TEST(test_rms_of_empty_buffer_is_zero);
    RUN_TEST(test_rms_of_constant_signal);
    RUN_TEST(test_rms_of_alternating_signal);

    RUN_TEST(test_loudness_zero_rms_returns_zero);
    RUN_TEST(test_loudness_below_floor_returns_zero);
    RUN_TEST(test_loudness_in_range_returns_positive_db);

    RUN_TEST(test_url_accepts_well_formed_https);
    RUN_TEST(test_url_rejects_http_by_default);
    RUN_TEST(test_url_rejects_other_schemes);
    RUN_TEST(test_url_rejects_userinfo);
    RUN_TEST(test_url_rejects_ip_literal_by_default);
    RUN_TEST(test_url_accepts_ip_literal_when_allowed);
    RUN_TEST(test_url_rejects_no_host);
    RUN_TEST(test_url_rejects_whitespace);
    RUN_TEST(test_url_rejects_oversized);
    RUN_TEST(test_url_rejects_bare_localhost);

    RUN_TEST(test_adpcm_round_trip_silence);
    RUN_TEST(test_adpcm_round_trip_ramp_is_close);
    RUN_TEST(test_adpcm_block_rejects_wrong_size);

    return UNITY_END();
}
