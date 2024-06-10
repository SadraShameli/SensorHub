#pragma once
#include <string>
#include <stack>

namespace Failsafe
{
    class Failure
    {
    public:
        Failure(const char *caller, std::string &&message) : Caller(caller), Message(std::move(message)) {}
        Failure(Failure &&other)
        {
            Caller = other.Caller;
            Message = std::move(other.Message);
        };

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