#include "Accessories.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "pins.h"

namespace {

constexpr uint8_t REG_MODE1 = 0x00;
constexpr uint8_t MODE1_AUTO_INCREMENT = 0x20;  // SLEEP and ALLCALL off
constexpr uint8_t REG_LED0_ON_L = 0x06;
constexpr uint8_t FULL_BIT = 0x10;  // bit 4 of LEDn_ON_H / LEDn_OFF_H

// Channel fully on: ON_H full bit set, OFF_H full bit clear (full-off wins if both are set).
bool channelFullOn(uint8_t channel) {
  Wire.beginTransmission(i2c::PCA9685);
  Wire.write((uint8_t)(REG_LED0_ON_L + 4 * channel));
  Wire.write((uint8_t)0x00);
  Wire.write(FULL_BIT);
  Wire.write((uint8_t)0x00);
  Wire.write((uint8_t)0x00);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool Accessories::begin() {
  lastFanMs_ = millis();
  return commandFans();
}

bool Accessories::commandFans() {
  Wire.beginTransmission(i2c::PCA9685);
  Wire.write(REG_MODE1);
  Wire.write(MODE1_AUTO_INCREMENT);
  bool ok = Wire.endTransmission() == 0;
  // Waking from sleep, the oscillator needs 500 us before outputs follow the
  // registers; the setting is kept, so it takes effect then without waiting here.
  ok = ok && channelFullOn(pca::FANS);
  fansOn_ = ok;
  return ok;
}

void Accessories::update(bool /*horn*/, float /*speed*/, bool /*batteryLow*/) {
  uint32_t now = millis();
  if ((uint32_t)(now - lastFanMs_) >= cfg::FAN_REFRESH_MS) {
    lastFanMs_ = now;
    commandFans();
  }
}
