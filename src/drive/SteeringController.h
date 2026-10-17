// Steering: AS5600 angle sensor on the column plus rideon::Pid, driving the
// fifth BTS7960 board. (later: build phase 5)
//
// Until the AS5600 is fitted and calibrated this holds the steering driver
// disabled, so the motor freewheels and the car is steered by hand.
// Parent mode therefore has throttle only for now.
#pragma once

class KillLines;

class SteeringController {
 public:
  bool begin();
  // steering: -1 (full left) .. +1 (full right) from the radio; ignored for now.
  void update(float steering, bool parentMode, float dtS, KillLines& lines);
  void stopNow(KillLines& lines);
};
