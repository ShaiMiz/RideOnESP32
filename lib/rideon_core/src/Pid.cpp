#include "Pid.h"

namespace rideon {

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float Pid::update(float setpoint, float measured, float dtS, const PidParams& p) {
  float error = setpoint - measured;
  integral_ = clampf(integral_ + error * dtS, -p.integralLimit, p.integralLimit);
  float derivative = (first_ || dtS <= 0.0f) ? 0.0f : (error - lastError_) / dtS;
  first_ = false;
  lastError_ = error;
  return clampf(p.kp * error + p.ki * integral_ + p.kd * derivative, p.outMin, p.outMax);
}

void Pid::reset() {
  integral_ = 0.0f;
  lastError_ = 0.0f;
  first_ = true;
}

float slewToward(float current, float target, float ratePerS, float dtS) {
  float maxStep = ratePerS * dtS;
  float diff = clampf(target - current, -maxStep, maxStep);
  return current + diff;
}

}  // namespace rideon
