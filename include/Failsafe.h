#pragma once
#include <string>
#include <stack>

class Failsafe
{
public:
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

    static void Init();
    static void Update();
    static void AddFailure(const char *, std::string &&);
    static void AddFailureDelayed(const char *, std::string &&);
    static void PopFailure();
    static const std::stack<Failsafe::Failure> &GetFailures() { return m_Failures; }

private:
    inline static std::stack<Failsafe::Failure> m_Failures;
};