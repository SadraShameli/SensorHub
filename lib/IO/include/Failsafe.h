#pragma once
#include <string>

class Failsafe
{
public:
    struct Failure
    {
        std::string Message, Error;
    };

public:
    static void Init();
    static void Update();
    static void AddFailure(const Failure &);
};