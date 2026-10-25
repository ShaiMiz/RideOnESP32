#include "DriveController.h"

#include "config.h"
#include "pins.h"

DriveController::DriveController()
    : motors_{{"FL", pins::FL_RPWM, pins::FL_LPWM, cfg::WHEEL_REVERSED[FL]},
              {"FR", pins::FR_RPWM, pins::FR_LPWM, cfg::WHEEL_REVERSED[FR]},
              {"RL", pins::RL_RPWM, pins::RL_LPWM, cfg::WHEEL_REVERSED[RL]},
              {"RR", pins::RR_RPWM, pins::RR_LPWM, cfg::WHEEL_REVERSED[RR]}} {}

bool DriveController::begin() {
  bool ok = true;
  for (auto& m : motors_) ok &= m.begin(cfg::PWM_FREQ_HZ, cfg::PWM_BITS);
  return ok;
}

void DriveController::update(float command, float dtS, const rideon::RampParams& ramp) {
  float v = ramp_.step(command, dtS, ramp);
  for (auto& m : motors_) m.write(v);
}

void DriveController::stopNow() {
  ramp_.reset();
  for (auto& m : motors_) m.write(0.0f);
}
