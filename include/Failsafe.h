#pragma once
#include <string>
#include <stack>

class Failsafe
{
public:
    class Failure
    {
    public:
        Failure(const char *caller, const std::string &message) : Caller(caller), Message(message) {}

        const char *Caller;
        std::string Message;
    };

    static void Init();
    static void Update();
    static void AddFailure(const char *, const std::string &);
    static void PopFailure();
    static const std::stack<Failsafe::Failure> &GetFailures() { return m_Failures; }

private:
    inline static std::stack<Failsafe::Failure> m_Failures;
};