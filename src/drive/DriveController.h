// Writes the ramped speed (rideon::Controller) to the four wheel outputs.
//
// For now every wheel gets the same speed, flipped per wheel by
// cfg::WHEEL_REVERSED so a mirrored motor still drives the car forward. Per-wheel output for
// traction control comes with the wheel-speed sensors.
#pragma once

#include "BTS7960Motor.h"

class DriveController {
 public:
  static constexpr int WHEELS = 4;
  enum Wheel { FL, FR, RL, RR };

  DriveController();
  bool begin();

  // Write `speed` (-1..1, already ramped and capped) to every wheel.
  void write(float speed);

 private:
  BTS7960Motor motors_[WHEELS];
};
