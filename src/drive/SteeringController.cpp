#include "SteeringController.h"

#include <Arduino.h>

#include "config.h"
#include "pins.h"
#include "safety/KillLines.h"

bool SteeringController::begin() {
  bool ok = ledcAttach(pins::STEER_RPWM, cfg::PWM_FREQ_HZ, cfg::PWM_BITS) &&
            ledcAttach(pins::STEER_LPWM, cfg::PWM_FREQ_HZ, cfg::PWM_BITS);
  ledcWrite(pins::STEER_RPWM, 0);
  ledcWrite(pins::STEER_LPWM, 0);
  return ok;
}

void SteeringController::update(float, bool, float, KillLines& lines) {
  // TODO(phase 5): read AS5600, map the stick to a target angle, run the PID
  // with a rate limit, enable only in Parent mode.
  stopNow(lines);
}

void SteeringController::stopNow(KillLines& lines) {
  ledcWrite(pins::STEER_RPWM, 0);
  ledcWrite(pins::STEER_LPWM, 0);
  lines.setSteer(false);
}
