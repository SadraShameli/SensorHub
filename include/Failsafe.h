#pragma once
#include <string>

class Failsafe
{
public:
    static void Init();
    static void Update();
    static void AddFailure(const std::string &);
};