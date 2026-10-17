#include "KillLines.h"

#include <Arduino.h>

#include "pins.h"

void KillLines::begin() {
  digitalWrite(pins::DRIVE_EN, LOW);
  digitalWrite(pins::STEER_EN, LOW);
  digitalWrite(pins::CONTACTOR_HOLD, LOW);
  pinMode(pins::DRIVE_EN, OUTPUT);
  pinMode(pins::STEER_EN, OUTPUT);
  pinMode(pins::CONTACTOR_HOLD, OUTPUT);
  allOff();
}

void KillLines::setDrive(bool on) {
  drive_ = on;
  digitalWrite(pins::DRIVE_EN, on ? HIGH : LOW);
}

void KillLines::setSteer(bool on) {
  steer_ = on;
  digitalWrite(pins::STEER_EN, on ? HIGH : LOW);
}

void KillLines::setContactor(bool closed) {
  contactor_ = closed;
  digitalWrite(pins::CONTACTOR_HOLD, closed ? HIGH : LOW);
}

void KillLines::allOff() {
  setDrive(false);
  setSteer(false);
  setContactor(false);
}
