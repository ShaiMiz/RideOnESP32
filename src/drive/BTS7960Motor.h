// One BTS7960 (IBT-2) half-bridge pair driving one motor.
//
// Ported from the v2.2 sketch's BTS7960Motor class. Ramping moved to
// rideon::Ramp and current reading to PowerMonitor (through the ADS1115), so
// this class only turns a signed speed into PWM.
//
// Output 0 drives both inputs LOW. With EN high that turns both low-side
// switches on, which brakes the motor. With EN low the motor coasts.
#pragma once

#include <stdint.h>

class BTS7960Motor {
 public:
  BTS7960Motor(const char* name, uint8_t rpwmPin, uint8_t lpwmPin)
      : name_(name), rpwm_(rpwmPin), lpwm_(lpwmPin) {}

  // Attaches both PWM pins and writes 0. Returns false if either fails.
  bool begin(uint32_t freqHz, uint8_t bits);

  // -1 (full reverse) .. +1 (full forward).
  void write(float speed);

  float output() const { return output_; }
  const char* name() const { return name_; }

 private:
  const char* name_;
  uint8_t rpwm_;
  uint8_t lpwm_;
  uint32_t maxDuty_ = 0;
  float output_ = 0.0f;
};
