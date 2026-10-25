// Box over-temperature with hysteresis.
//
// Hot once the box reads above limitC for holdMs, and stays hot until it
// cools below recoverC. While the sensor is required, a missing reading
// (none good for staleMs) also counts as hot: an unknown temperature is not
// a safe one.
#pragma once

#include <stdint.h>

#include "Faults.h"

namespace rideon {

struct BoxTempParams {
  float limitC;
  float recoverC;
  uint32_t holdMs;
  uint32_t staleMs;
  bool required;  // false until the DS18B20 is fitted
};

class BoxTempCheck {
 public:
  // lastGoodMs: when the last good reading came. hasReading: one ever came.
  bool update(bool hasReading, float celsius, uint32_t lastGoodMs, uint32_t nowMs, const BoxTempParams& p);

  bool hot() const { return hot_; }
  bool sensorLost() const { return lost_; }

 private:
  bool overTemp_ = false;
  bool hot_ = false;
  bool lost_ = false;
  HoldTimer above_;
};

}  // namespace rideon
