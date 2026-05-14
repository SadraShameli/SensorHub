#pragma once

#include <string_view>

namespace sensorhub::core {

struct UrlValidatorOptions {
    bool require_https = true;
    bool allow_ip_literal = false;
    bool require_dot_in_host = true;
};

inline bool IsAllowedBackendUrl(std::string_view url,
                                const UrlValidatorOptions& opts = {}) {
    if (url.empty() || url.size() > 240) {
        return false;
    }

    for (char c : url) {
        if (c <= 0x20 || c == 0x7F) {
            return false;
        }
    }

    std::string_view rest;
    if (opts.require_https) {
        constexpr std::string_view kHttps = "https://";
        if (url.substr(0, kHttps.size()) != kHttps) {
            return false;
        }
        rest = url.substr(kHttps.size());
    } else {
        constexpr std::string_view kHttp = "http://";
        constexpr std::string_view kHttps = "https://";
        if (url.substr(0, kHttps.size()) == kHttps) {
            rest = url.substr(kHttps.size());
        } else if (url.substr(0, kHttp.size()) == kHttp) {
            rest = url.substr(kHttp.size());
        } else {
            return false;
        }
    }

    if (rest.empty()) {
        return false;
    }

    const auto at = rest.find('@');
    const auto slash = rest.find('/');
    if (at != std::string_view::npos &&
        (slash == std::string_view::npos || at < slash)) {
        return false;
    }

    auto end = rest.size();
    for (auto c : {':', '/', '?', '#'}) {
        auto pos = rest.find(c);
        if (pos != std::string_view::npos && pos < end) {
            end = pos;
        }
    }
    const std::string_view host = rest.substr(0, end);

    if (host.empty()) {
        return false;
    }

    if (!opts.allow_ip_literal) {
        bool all_digits_or_dots = true;
        for (char c : host) {
            if (!((c >= '0' && c <= '9') || c == '.')) {
                all_digits_or_dots = false;
                break;
            }
        }
        if (all_digits_or_dots) {
            return false;
        }
    }

    if (opts.require_dot_in_host && host.find('.') == std::string_view::npos) {
        return false;
    }

    return true;
}

}
