#pragma once

#include "esp_log.h"

#define SH_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define SH_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define SH_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define SH_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)

#ifdef UNIT_DEBUG

#define SH_LOGSECRET(tag, label, value) \
    ESP_LOGD(tag, "%s: %s", (label), (value))
#else

#include <cstring>
#define SH_LOGSECRET(tag, label, value) \
    ESP_LOGI(tag, "%s: <%u chars>", (label), (unsigned)std::strlen(value))
#endif
