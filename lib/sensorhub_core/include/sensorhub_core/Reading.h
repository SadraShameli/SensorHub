#pragma once

#include <atomic>
#include <limits>

namespace sensorhub::core {

class Reading {
   public:
    Reading()
        : m_Current(0.0f),
          m_Min(std::numeric_limits<float>::max()),
          m_Max(std::numeric_limits<float>::lowest()) {}

    Reading(float current, float min, float max)
        : m_Current(current), m_Min(min), m_Max(max) {}

    Reading(const Reading& other)
        : m_Current(other.m_Current.load(std::memory_order_relaxed)),
          m_Min(other.m_Min.load(std::memory_order_relaxed)),
          m_Max(other.m_Max.load(std::memory_order_relaxed)) {}

    Reading& operator=(const Reading& other) {
        if (this != &other) {
            m_Current.store(other.m_Current.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
            m_Min.store(other.m_Min.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
            m_Max.store(other.m_Max.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
        }
        return *this;
    }

    float Current() const { return m_Current.load(std::memory_order_relaxed); }

    float Min() const { return m_Min.load(std::memory_order_relaxed); }

    float Max() const { return m_Max.load(std::memory_order_relaxed); }

    void Reset() {
        const float c = m_Current.load(std::memory_order_relaxed);
        m_Min.store(c, std::memory_order_relaxed);
        m_Max.store(c, std::memory_order_relaxed);
    }

    void Update(float current) {
        m_Current.store(current, std::memory_order_relaxed);

        float prev = m_Min.load(std::memory_order_relaxed);
        while (current < prev &&
               !m_Min.compare_exchange_weak(prev,
                                            current,
                                            std::memory_order_relaxed)) {}

        prev = m_Max.load(std::memory_order_relaxed);
        while (current > prev &&
               !m_Max.compare_exchange_weak(prev,
                                            current,
                                            std::memory_order_relaxed)) {}
    }

   private:
    std::atomic<float> m_Current;
    std::atomic<float> m_Min;
    std::atomic<float> m_Max;
};

}
