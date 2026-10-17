// Battery voltage classification with hysteresis.
//
// Readings outside the plausible range are a sensor fault, never "OK": the
// old firmware treated a broken divider as a healthy battery.
#pragma once

#include <stdint.h>

#include "Faults.h"

namespace rideon {

enum class BatteryState : uint8_t { Ok, Low, Cutoff, SensorFault };

struct BatteryParams {
  float sensorMinV;
  float sensorMaxV;
  float lowWarnV;
  float cutoffV;
  float recoverV;
  float highWarnV;
  uint32_t cutoffHoldMs;
};

class BatteryCheck {
 public:
  BatteryState update(float volts, uint32_t nowMs, const BatteryParams& p);

  BatteryState state() const { return state_; }
  bool highWarning() const { return high_; }

 private:
  BatteryState state_ = BatteryState::Ok;
  bool cutoff_ = false;
  bool high_ = false;
  HoldTimer below_;
};

}  // namespace rideon
