#include "BTS7960Motor.h"

#include <Arduino.h>

bool BTS7960Motor::begin(uint32_t freqHz, uint8_t bits) {
  maxDuty_ = (1u << bits) - 1;
  bool ok = ledcAttach(rpwm_, freqHz, bits) && ledcAttach(lpwm_, freqHz, bits);
  ledcWrite(rpwm_, 0);
  ledcWrite(lpwm_, 0);
  output_ = 0.0f;
  return ok;
}

void BTS7960Motor::write(float speed) {
  if (speed > 1.0f) speed = 1.0f;
  if (speed < -1.0f) speed = -1.0f;
  output_ = speed;
  uint32_t duty = (uint32_t)((speed < 0 ? -speed : speed) * maxDuty_ + 0.5f);
  // Zero the side being released first, so both inputs are never high together.
  if (speed > 0.0f) {
    ledcWrite(lpwm_, 0);
    ledcWrite(rpwm_, duty);
  } else if (speed < 0.0f) {
    ledcWrite(rpwm_, 0);
    ledcWrite(lpwm_, duty);
  } else {
    ledcWrite(rpwm_, 0);
    ledcWrite(lpwm_, 0);
  }
}
