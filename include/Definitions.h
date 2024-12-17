#pragma once

#include <filesystem>
#include <limits>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#ifdef UNIT_DEBUG
#define UNIT_TIMER(msg) Helpers::ProfileScope scope(msg)
#else
#define UNIT_TIMER(msg)
#endif

/**
 * @namespace
 * @brief A namespace for helper functions.
 */
namespace Helpers {

/**
 * @class
 * @brief A class for profiling code execution.
 */
class ProfileScope {
   public:
    /**
     * @brief Constructs a new profile scope with the given name.
     *
     * @param name The name of the profile scope.
     */
    ProfileScope(const char* name)
        : m_Name(name),
          m_StartTime(esp_timer_get_time()),
          m_StartHeap(esp_get_free_heap_size()) {}

    /**
     * @brief Destructs the profile scope and logs the time taken.
     */
    ~ProfileScope() {
        ESP_LOGI("Scope", "%s took %ld ms - heap before: %ld - heap after: %ld",
                 m_Name, (long int)(esp_timer_get_time() - m_StartTime) / 1000,
                 m_StartHeap, esp_get_free_heap_size());
    }

   private:
    const char* m_Name;
    int64_t m_StartTime;
    uint32_t m_StartHeap;
};

/**
 * @brief Removes trailing whitespace characters from the given container.
 *
 * This function removes any trailing spaces, newlines, carriage returns,
 * and tab characters from the end of the provided container.
 *
 * @param t The container from which to remove trailing whitespace characters.
 */
template <typename T>
inline void RemoveWhiteSpace(T& t) {
    t.erase(t.find_last_not_of(" \n\r\t") + 1);
}

/**
 * @brief Maps a value from one range to another.
 *
 * This function maps a value from one range to another. The input value `x`
 * is mapped from the range `[in_min, in_max]` to the range `[out_min,
 * out_max]`.
 *
 * @tparam T The type of the input and output values.
 * @param x The value to map.
 * @param in_min The minimum value of the input range.
 * @param in_max The maximum value of the input range.
 * @param out_min The minimum value of the output range.
 * @param out_max The maximum value of the output range.
 * @return The mapped value.
 */
template <typename T>
inline long MapValue(T x, T in_min, T in_max, T out_min, T out_max) {
    const T run = in_max - in_min;
    if (run == 0) {
        return -1;
    }
    const T rise = out_max - out_min;
    const T delta = x - in_min;
    return (delta * rise) / run + out_min;
}

/**
 * @brief Prints the current free heap size.
 */
inline void PrintFreeHeap() {
    ESP_LOGI("Heap", "Free: %ld", esp_get_free_heap_size());
}

/**
 * @brief Gets the size of the file at the given path.
 *
 * This function returns the size of the file at the given path.
 *
 * @param filePath The path to the file.
 * @return The size of the file in bytes.
 */
inline uint32_t GetFileSize(const char* filePath) {
    return (uint32_t)std::filesystem::file_size(filePath);
}

}  // namespace Helpers

/**
 * @class
 * @brief A class for storing readings.
 */
class Reading {
   public:
    /**
     * @brief Constructs a new reading with a current value of 0.
     */
    Reading()
        : m_Current(0),
          m_Min(std::numeric_limits<float>::max()),
          m_Max(std::numeric_limits<float>::min()) {}

    /**
     * @brief Constructs a new reading with the given current value.
     *
     * @param current The current value of the reading.
     */
    Reading(float current, float min, float max)
        : m_Current(current), m_Min(min), m_Max(max) {}

    /**
     * @brief Gets the current value of the reading.
     *
     * @return The current value of the reading.
     */
    float Current() const { return m_Current; }

    /**
     * @brief Gets the minimum value of the reading.
     *
     * @return The minimum value of the reading.
     */
    float Min() const { return m_Min; }

    /**
     * @brief Gets the maximum value of the reading.
     *
     * @return The maximum value of the reading.
     */
    float Max() const { return m_Max; }

    /**
     * @brief Resets the reading to the current value.
     */
    void Reset() { m_Min = m_Max = m_Current; }

    /**
     * @brief Updates the reading with the given value.
     *
     * This function updates the reading with the given value. If the value is
     * less than the current minimum, it becomes the new minimum. If the value
     * is greater than the current maximum, it becomes the new maximum.
     *
     * @param current The new value to update the reading with.
     */
    void Update(float current) {
        m_Current = current;
        if ((current < m_Min)) {
            m_Min = current;
        }

        if (current > m_Max) {
            m_Max = current;
        }
    }

   private:
    float m_Current, m_Min, m_Max;
};