#pragma once

#include <optional>
#include <string>

#include "core/Service.h"

namespace Failsafe {

extern const Kernel::Service kService;

class Failure {
   public:
    Failure() = default;

    Failure(const char* caller, const char* message)
        : Caller(caller), Message(message) {}

    Failure(const char* caller, const std::string& message)
        : Caller(caller), Message(message) {}

    Failure(const char* caller, std::string&& message)
        : Caller(caller), Message(std::move(message)) {}

    const char* Caller = nullptr;
    std::string Message;
};

void Init();
void Update();

void AddFailure(const char*, std::string&&);
void AddFailureDelayed(const char*, std::string&&);
void PopFailure();

std::optional<Failure> PeekTopFailure();
size_t FailureCount();

};