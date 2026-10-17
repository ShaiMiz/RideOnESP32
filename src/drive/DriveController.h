// Turns the active mode's speed command into the four wheel outputs.
//
// For now every wheel gets the same ramped speed. Per-wheel output for
// traction control comes with the wheel-speed sensors.
#pragma once

#include "BTS7960Motor.h"
#include "Ramp.h"

class DriveController {
 public:
  static constexpr int WHEELS = 4;
  enum Wheel { FL, FR, RL, RR };

  DriveController();
  bool begin();

  // Ramp toward `command` (-1..1, already capped) and write all wheels.
  void update(float command, float dtS, const rideon::RampParams& ramp);

  // Zero every output at once, no ramp. Used with the drive enable dropped.
  void stopNow();

  float speed() const { return ramp_.value(); }
  bool stopped(const rideon::RampParams& ramp) const { return ramp_.stopped(ramp); }

 private:
  BTS7960Motor motors_[WHEELS];
  rideon::Ramp ramp_;
};
