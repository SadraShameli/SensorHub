#pragma once
#include <string>

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
};