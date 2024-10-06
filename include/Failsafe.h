#pragma once

#include <stack>
#include <string>

namespace Failsafe
{
    class Failure
    {
    public:
        Failure(const char *caller, const char *message) : Caller(caller), Message(message) {}
        Failure(const char *caller, const std::string &message) : Caller(caller), Message(message) {}
        Failure(const char *caller, std::string &&message) : Caller(caller), Message(std::move(message)) {}

        const char *Caller;
        std::string Message;
    };

    void Init();
    void Update();

    void AddFailure(const char *, std::string &&);
    void AddFailureDelayed(const char *, std::string &&);
    void PopFailure();

    const std::stack<Failsafe::Failure> &GetFailures();
};