#pragma once
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
        ~ProfileScope()
        {
            ESP_LOGI("Scope", "%s took %ld ms - heap before: %ld - heap after: %ld", m_Name, (long int)(esp_timer_get_time() - m_StartTime) / 1000, m_StartHeap, esp_get_free_heap_size());
        }

    private:
        const char *m_Name;
        int64_t m_StartTime;
        uint32_t m_StartHeap;
    };

    template <typename T>
    inline static void RemoveWhiteSpace(T &t) { t.erase(t.find_last_not_of(" \n\r\t") + 1); }

    inline static long MapValue(long x, long in_min, long in_max, long out_min, long out_max)
    {
        const long run = in_max - in_min;
        if (run == 0)
        {
            return -1;
        }
        const long rise = out_max - out_min;
        const long delta = x - in_min;
        return (delta * rise) / run + out_min;
    }

    inline static void PrintFreeHeap() { ESP_LOGI("Heap", "Free: %ld", esp_get_free_heap_size()); }
    inline static size_t GetFileSize(const char *filePath) { return std::filesystem::file_size(filePath); }
}

class Reading
{
public:
    Reading(float current, float min, float max) : m_Current(current), m_Min(min), m_Max(max) {}
    float GetCurrent() const { return m_Current; }
    float GetMin() const { return m_Min; }
    float GetMax() const { return m_Max; }
    void Reset() { m_Max = m_Min = m_Current; }
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