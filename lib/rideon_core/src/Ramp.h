// Speed ramping: gentle acceleration, firm braking, and no change of
// direction until the car has stopped.
//
// Values are fractions of full speed, -1 (full reverse) to +1 (full forward).
#pragma once

namespace rideon {

struct RampParams {
  float accelPerS;     // rate when speeding up
  float brakePerS;     // rate when slowing down
  float stoppedEps;    // below this counts as stopped
};

class Ramp {
 public:
  // Move toward `target` by at most one step and return the new value.
  float step(float target, float dtS, const RampParams& p);

  float value() const { return current_; }
  bool stopped(const RampParams& p) const;
  void reset() { current_ = 0.0f; }

 private:
  float current_ = 0.0f;
};

// Apply the speed cap, and cap reverse to `reverseCap` of it.
float limitCommand(float cmd, float cap, float reverseCap);

}  // namespace rideon
