#pragma once

#include <cstdint>
#include <string>

#include "Configuration.h"
#include "core/Service.h"

namespace Storage {

extern const Kernel::Service kService;

void Init();

void Commit();
void Reset();
void Mount(const char* base_path, const char* partition_label);

const std::string& GetSSID();
const std::string& GetPassword();
const std::string& GetAddress();
const std::string& GetAuthKey();
const std::string& GetDeviceName();
const std::string& GetApPassword();
uint32_t GetDeviceId();
uint32_t GetLoudnessThreshold();
uint32_t GetRegisterInterval();
bool GetSensorState(Configuration::Sensor::Sensors);
bool GetConfigMode();

void SetSSID(std::string&&);
void SetPassword(std::string&&);
void SetAddress(std::string&&);
void SetAuthKey(std::string&&);
void SetDeviceName(std::string&&);
void SetApPassword(std::string&&);
void SetDeviceId(uint32_t);
void SetLoudnessThreshold(uint32_t);
void SetRegisterInterval(uint32_t);
void SetSensorState(Configuration::Sensor::Sensors, bool);
void SetConfigMode(bool);

}
