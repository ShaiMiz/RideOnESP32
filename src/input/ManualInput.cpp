#include "ManualInput.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "pins.h"

namespace {
bool readExpander(uint8_t& value) {
  if (Wire.requestFrom(i2c::PCF8574, (uint8_t)1) != 1) return false;
  value = (uint8_t)Wire.read();
  return true;
}
bool pressed(uint8_t value, uint8_t bit) { return (value & (1u << bit)) == 0; }  // switches pull to GND
}  // namespace

bool ManualInput::begin() {
  pinMode(pins::PEDAL, INPUT);
  Wire.beginTransmission(i2c::PCF8574);
  Wire.write(0xFF);  // all pins as inputs (quasi-bidirectional, weak pull-up)
  ok_ = Wire.endTransmission() == 0;
  return ok_;
}

void ManualInput::update() {
  uint32_t mv = analogReadMilliVolts(pins::PEDAL);
  float p = ((float)mv - cfg::PEDAL_RELEASED_MV) / (float)(cfg::PEDAL_FULL_MV - cfg::PEDAL_RELEASED_MV);
  pedal_ = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);

  uint8_t v;
  ok_ = readExpander(v);
  if (!ok_) {
    direction_ = 0;
    horn_ = false;
    return;
  }
  bool fwd = pressed(v, pcf::SHIFT_FORWARD);
  bool rev = pressed(v, pcf::SHIFT_REVERSE);
  direction_ = (fwd && !rev) ? 1 : ((rev && !fwd) ? -1 : 0);  // both = wiring fault = neutral
  horn_ = pressed(v, pcf::HORN);
  estop_ = pressed(v, pcf::ESTOP_SENSE);
  seat_ = pressed(v, pcf::SEAT);
}
