#pragma once

#include <stack>
#include <string>

/**
 * @namespace
 * @brief A namespace for managing failures.
 */
namespace Failsafe {

/**
 * @class
 * @brief Represents a failure with a caller and a message.
 *
 * The Failure class encapsulates information about a failure, including the
 * caller and a message describing the failure.
 */
class Failure {
   public:
    Failure(const char *caller, const char *message)
        : Caller(caller), Message(message) {}

    Failure(const char *caller, const std::string &message)
        : Caller(caller), Message(message) {}

    Failure(const char *caller, std::string &&message)
        : Caller(caller), Message(std::move(message)) {}

    const char *Caller;
    std::string Message;
};

void Init();
void Update();

void AddFailure(const char *, std::string &&);
void AddFailureDelayed(const char *, std::string &&);
void PopFailure();

const std::stack<Failsafe::Failure> &GetFailures();

};  // namespace Failsafe