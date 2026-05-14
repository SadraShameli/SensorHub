#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sensorhub_core/ImaAdpcm.h"

namespace Mic {

namespace detail {
constexpr uint32_t CeilBlocks(uint32_t seconds, uint32_t sampleRateHz,
                              uint32_t samplesPerBlock) {
    return (seconds * sampleRateHz + samplesPerBlock - 1) / samplesPerBlock;
}
}

struct AdpcmConfig {
    static constexpr uint32_t SampleRateHz = 32000;
    static constexpr uint32_t PreRollSeconds = 5;
    static constexpr uint32_t PostRollSeconds = 10;
    static constexpr uint32_t JitterSlackSeconds = 3;

    static constexpr uint16_t BlockAlign = 256;
    static constexpr uint16_t SamplesPerBlock = 505;

    static constexpr uint32_t PreRollBlocks =
        detail::CeilBlocks(PreRollSeconds, SampleRateHz, SamplesPerBlock);
    static constexpr uint32_t PostRollBlocks =
        detail::CeilBlocks(PostRollSeconds, SampleRateHz, SamplesPerBlock);
    static constexpr uint32_t JitterSlackBlocks =
        detail::CeilBlocks(JitterSlackSeconds, SampleRateHz, SamplesPerBlock);
    static constexpr uint32_t RingCapacityBlocks =
        PreRollBlocks + JitterSlackBlocks;
    static constexpr uint32_t PcmBytesPerBlock =
        SamplesPerBlock * sizeof(int16_t);
};

struct __attribute__((packed)) WavHeaderImaAdpcm {
    uint8_t RiffTag[4] = {'R', 'I', 'F', 'F'};
    uint32_t FileLength;
    uint8_t WaveTag[4] = {'W', 'A', 'V', 'E'};

    uint8_t FmtTag[4] = {'f', 'm', 't', ' '};
    uint32_t FmtChunkSize = 20;
    uint16_t FormatTag = 0x0011;
    uint16_t ChannelCount = 1;
    uint32_t SampleRate;
    uint32_t BytesPerSecond;
    uint16_t BlockAlign = AdpcmConfig::BlockAlign;
    uint16_t BitsPerSample = 4;
    uint16_t CbSize = 2;
    uint16_t SamplesPerBlock = AdpcmConfig::SamplesPerBlock;

    uint8_t FactTag[4] = {'f', 'a', 'c', 't'};
    uint32_t FactChunkSize = 4;
    uint32_t NumSamples;

    uint8_t DataTag[4] = {'d', 'a', 't', 'a'};
    uint32_t DataLength;

    WavHeaderImaAdpcm(uint32_t sampleRate, uint32_t totalBlocks) {
        SampleRate = sampleRate;
        BytesPerSecond = (sampleRate * AdpcmConfig::BlockAlign) /
                         AdpcmConfig::SamplesPerBlock;
        NumSamples = totalBlocks * AdpcmConfig::SamplesPerBlock;
        DataLength = totalBlocks * AdpcmConfig::BlockAlign;
        FileLength =
            static_cast<uint32_t>(sizeof(WavHeaderImaAdpcm)) - 8 + DataLength;
    }
};

static_assert(sizeof(WavHeaderImaAdpcm) == 60,
              "IMA ADPCM WAV header must be 60 bytes");

class AdpcmEncoderState {
   public:
    AdpcmEncoderState() {
        m_pcm = static_cast<int16_t*>(
            heap_caps_malloc(AdpcmConfig::PcmBytesPerBlock,
                             MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
        if (m_pcm == nullptr) {
            ESP_LOGE("AdpcmEnc",
                     "PCM scratch alloc failed (%u bytes)",
                     (unsigned)AdpcmConfig::PcmBytesPerBlock);
            std::abort();
        }
    }

    ~AdpcmEncoderState() { heap_caps_free(m_pcm); }

    AdpcmEncoderState(const AdpcmEncoderState&) = delete;
    AdpcmEncoderState& operator=(const AdpcmEncoderState&) = delete;

    int16_t* PcmBuffer() { return m_pcm; }

    size_t PcmBufferBytes() const { return AdpcmConfig::PcmBytesPerBlock; }

    bool Encode(uint8_t* outBlock) {
        return m_encoder.EncodeWavBlock(m_pcm,
                                        AdpcmConfig::SamplesPerBlock,
                                        outBlock);
    }

   private:
    sensorhub::core::ImaAdpcmEncoder m_encoder;
    int16_t* m_pcm = nullptr;
};

class AdpcmRing {
   public:
    enum class State : uint8_t { Idle, Recording, PostCaptureDrain };

    explicit AdpcmRing(uint8_t* backing) : m_buf(backing) {}

    ~AdpcmRing() = default;

    AdpcmRing(const AdpcmRing&) = delete;
    AdpcmRing& operator=(const AdpcmRing&) = delete;

    State CurrentState() const {
        return m_state.load(std::memory_order_acquire);
    }

    uint32_t PostRollProduced() const {
        return m_postRollProduced.load(std::memory_order_acquire);
    }

    uint32_t Size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    void PushOverwrite(const uint8_t* block) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state.load(std::memory_order_relaxed) != State::Idle) {
            return;
        }
        WriteBlockLocked(block);
        if (m_count < AdpcmConfig::PreRollBlocks) {
            ++m_count;
        } else {
            m_tail = (m_tail + 1) % AdpcmConfig::RingCapacityBlocks;
        }
    }

    bool PushPreserve(const uint8_t* block) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state.load(std::memory_order_relaxed) != State::Recording) {
            return false;
        }
        if (m_count >= AdpcmConfig::RingCapacityBlocks) {
            ++m_droppedDuringRecording;
            return false;
        }
        WriteBlockLocked(block);
        ++m_count;
        m_postRollProduced.fetch_add(1, std::memory_order_release);
        return true;
    }

    uint32_t DroppedDuringRecording() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_droppedDuringRecording;
    }

    bool Pop(uint8_t* outBlock) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_count == 0) {
            return false;
        }
        std::memcpy(outBlock,
                    m_buf + m_tail * AdpcmConfig::BlockAlign,
                    AdpcmConfig::BlockAlign);
        m_tail = (m_tail + 1) % AdpcmConfig::RingCapacityBlocks;
        --m_count;
        return true;
    }

    uint32_t BeginRecording() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state.load(std::memory_order_relaxed) != State::Idle) {
            return 0;
        }
        const uint32_t preRoll = m_count;
        m_droppedDuringRecording = 0;
        m_postRollProduced.store(0, std::memory_order_release);
        m_state.store(State::Recording, std::memory_order_release);
        return preRoll;
    }

    void MarkPostRollDone() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state.load(std::memory_order_relaxed) == State::Recording) {
            m_state.store(State::PostCaptureDrain, std::memory_order_release);
        }
    }

    void EndRecording() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_count = 0;
        m_postRollProduced.store(0, std::memory_order_release);
        m_state.store(State::Idle, std::memory_order_release);
    }

   private:
    void WriteBlockLocked(const uint8_t* block) {
        std::memcpy(m_buf + m_head * AdpcmConfig::BlockAlign,
                    block,
                    AdpcmConfig::BlockAlign);
        m_head = (m_head + 1) % AdpcmConfig::RingCapacityBlocks;
    }

    mutable std::mutex m_mutex;
    std::atomic<State> m_state{State::Idle};
    std::atomic<uint32_t> m_postRollProduced{0};

    uint8_t* m_buf = nullptr;
    uint32_t m_head = 0;
    uint32_t m_tail = 0;
    uint32_t m_count = 0;
    uint32_t m_droppedDuringRecording = 0;
};

}
