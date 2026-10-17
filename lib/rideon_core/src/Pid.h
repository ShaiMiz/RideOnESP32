// PID controller and slew limiter for the steering position loop.
#pragma once

namespace rideon {

struct PidParams {
  float kp;
  float ki;
  float kd;
  float outMin;
  float outMax;
  float integralLimit;
};

class Pid {
 public:
  float update(float setpoint, float measured, float dtS, const PidParams& p);
  void reset();

 private:
  float integral_ = 0.0f;
  float lastError_ = 0.0f;
  bool first_ = true;
};

// Move `current` toward `target` by at most ratePerS * dtS.
float slewToward(float current, float target, float ratePerS, float dtS);

}  // namespace rideon
