#pragma once

#include <cstdint>

#include "Definitions.h"

namespace Sensors {

class ISensor {
   public:
    virtual ~ISensor() = default;

    virtual const char* Name() const = 0;
    virtual const char* Unit() const = 0;
    virtual uint8_t Id() const = 0;
    virtual bool IsOk() const = 0;
    virtual Reading Snapshot() const = 0;
    virtual void ResetMinMax() = 0;

    virtual int ReportingValue() const {
        return static_cast<int>(Snapshot().Current());
    }
};

}
