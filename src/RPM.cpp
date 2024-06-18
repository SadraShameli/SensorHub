// #include <algorithm>
// #include "driver/gpio.h"
// #include "esp_timer.h"
// #include "RPM.h"

// namespace RPM
// {
//   static constexpr gpio_num_t rpm_pin = GPIO_NUM_4;
//   static const uint8_t PulsesPerRevolution = 2;
//   static const unsigned long ZeroTimeout = 100000;
//   static const uint8_t numReadings = 10;
//   static volatile unsigned long LastTimeWeMeasured;
//   static volatile unsigned long PeriodBetweenPulses = ZeroTimeout + 1000;
//   static volatile unsigned long PeriodAverage = ZeroTimeout + 1000;
//   static unsigned long FrequencyRaw;
//   static unsigned long FrequencyReal;
//   static unsigned long currentRPM;
//   static unsigned long maxRPM;
//   static int64_t LastTimeCycleMeasure;
//   static int64_t CurrentMicros;
//   static unsigned int ZeroDebouncingExtra;
//   static unsigned long readings[numReadings];
//   static unsigned long readIndex;
//   static unsigned long total;
//   static unsigned long avgRPM;
//   static unsigned int PulseCounter = 1;
//   static unsigned long PeriodSum;
//   static unsigned int AmountOfReadings = 1;

//   static void pulseEventHandler()
//   {
//     PeriodBetweenPulses = esp_timer_get_time() - LastTimeWeMeasured;
//     LastTimeWeMeasured = esp_timer_get_time();

//     if (PulseCounter >= AmountOfReadings)
//     {
//       PeriodAverage = PeriodSum / AmountOfReadings;

//       PulseCounter = 1;
//       PeriodSum = PeriodBetweenPulses;

//       int RemapedAmountOfReadings = Helpers::MapValue(PeriodBetweenPulses, 40000, 5000, 1, 10);
//       RemapedAmountOfReadings = std::clamp(RemapedAmountOfReadings, 1, 10);
//       AmountOfReadings = RemapedAmountOfReadings;
//     }
//     else
//     {
//       PulseCounter++;
//       PeriodSum = PeriodSum + PeriodBetweenPulses;
//     }
//   }

//   void Init()
//   {
//     gpio_set_direction(rpm_pin, GPIO_MODE_INPUT);
//     gpio_set_pull_mode(rpm_pin, GPIO_PULLUP_ONLY);
//   }

//   void Update()
//   {
//     LastTimeCycleMeasure = LastTimeWeMeasured;
//     CurrentMicros = esp_timer_get_time();

//     if (CurrentMicros < LastTimeCycleMeasure)
//     {
//       LastTimeCycleMeasure = CurrentMicros;
//     }

//     if (PeriodAverage != 0)
//     {
//       FrequencyRaw = 10000000000 / PeriodAverage;
//     }

//     if (PeriodBetweenPulses > ZeroTimeout - ZeroDebouncingExtra || CurrentMicros - LastTimeCycleMeasure > ZeroTimeout - ZeroDebouncingExtra)
//     {
//       FrequencyRaw = 0;
//       ZeroDebouncingExtra = 2000;
//     }

//     else
//     {
//       ZeroDebouncingExtra = 0;
//     }

//     FrequencyReal = FrequencyRaw / 10000;
//     currentRPM = FrequencyRaw / PulsesPerRevolution * 60;
//     currentRPM = currentRPM / 10000;

//     if (currentRPM > maxRPM && currentRPM <= 20000)
//     {
//       maxRPM = currentRPM;
//     }

//     total = total - readings[readIndex];
//     readings[readIndex] = currentRPM;
//     total = total + readings[readIndex];
//     readIndex = readIndex + 1;

//     if (readIndex >= numReadings)
//     {
//       readIndex = 0;
//     }

//     if (numReadings != 0)
//     {
//       avgRPM = total / numReadings;
//     }
//   }

//   unsigned long GetRPM() { return currentRPM; }
//   unsigned long GetMaxRPM() { return maxRPM; }
//   unsigned long GetAvgRPM() { return avgRPM; }

//   void ResetValues()
//   {
//     maxRPM = currentRPM;
//     avgRPM = currentRPM;
//   }
// }