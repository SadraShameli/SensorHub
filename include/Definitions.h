#pragma once
#include <limits>
#include <filesystem>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

#ifdef UNIT_DEBUG
#define UNIT_TIMER(msg) Helpers::ProfileScope scope(msg)
#else
#define UNIT_TIMER(msg)
#endif

namespace Helpers
{
    class ProfileScope
    {
    public:
        ProfileScope(const char *name) : m_Name(name), m_StartTime(esp_timer_get_time()), m_StartHeap(esp_get_free_heap_size()) {}
        ~ProfileScope() { ESP_LOGI("Scope", "%s took %ld ms - heap before: %ld - heap after: %ld", m_Name, (long int)(esp_timer_get_time() - m_StartTime) / 1000, m_StartHeap, esp_get_free_heap_size()); }

    private:
        const char *m_Name;
        int64_t m_StartTime;
        uint32_t m_StartHeap;
    };

    template <typename T>
    inline static void RemoveWhiteSpace(T &t) { t.erase(t.find_last_not_of(" \n\r\t") + 1); }

    template <typename T>
    inline static long MapValue(T x, T in_min, T in_max, T out_min, T out_max)
    {
        const T run = in_max - in_min;
        if (run == 0)
        {
            return -1;
        }
        const T rise = out_max - out_min;
        const T delta = x - in_min;
        return (delta * rise) / run + out_min;
    }

    inline static void PrintFreeHeap() { ESP_LOGI("Heap", "Free: %ld", esp_get_free_heap_size()); }
    inline static uint32_t GetFileSize(const char *filePath) { return (uint32_t)std::filesystem::file_size(filePath); }
}

class Reading
{
public:
    Reading() : m_Current(0), m_Min(std::numeric_limits<float>::max()), m_Max(std::numeric_limits<float>::min()) {}
    Reading(float current, float min, float max) : m_Current(current), m_Min(min), m_Max(max) {}

    float Current() const { return m_Current; }
    float Min() const { return m_Min; }
    float Max() const { return m_Max; }

    void Reset() { m_Min = m_Max = m_Current; }
    void Update(float current)
    {
        m_Current = current;
        if ((current < m_Min))
            m_Min = current;
        if (current > m_Max)
            m_Max = current;
    }

private:
    float m_Current, m_Min, m_Max;
};