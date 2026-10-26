#include "PowerMonitor.h"

#include <Arduino.h>
#include <Wire.h>

#include "Ads1115.h"
#include "config.h"
#include "pins.h"

using namespace rideon;

namespace {

bool writeRegister(uint8_t addr, uint8_t reg, uint16_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));
  Wire.write((uint8_t)(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t addr, uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return false;
  uint8_t hi = (uint8_t)Wire.read();
  uint8_t lo = (uint8_t)Wire.read();
  value = (uint16_t)((hi << 8) | lo);
  return true;
}

uint8_t addressFor(int ch) { return ch < 4 ? i2c::ADS_DRIVE : i2c::ADS_AUX; }

uint8_t inputFor(int ch) {
  if (ch < 4) return (uint8_t)ch;  // FL, FR, RL, RR on A0-A3
  return ch == PowerMonitor::STEER ? ads::STEER_IS : ads::MOTOR_BUS;
}

}  // namespace

bool PowerMonitor::begin(bool driveOnly) {
  driveOnly_ = driveOnly;
  pinMode(pins::BATTERY_SENSE, INPUT);
  analogSetPinAttenuation(pins::BATTERY_SENSE, ADC_11db);

  uint16_t unused;
  bool ok = readRegister(i2c::ADS_DRIVE, ads1115::REG_CONFIG, unused);
  if (!driveOnly_) ok &= readRegister(i2c::ADS_AUX, ads1115::REG_CONFIG, unused);

  uint32_t now = millis();
  for (uint32_t& t : lastSampleMs_) t = now;  // give every channel one stale period to report
  next_ = 0;
  startConversion(now);
  return ok;
}

void PowerMonitor::startConversion(uint32_t nowMs) {
  converting_ = writeRegister(addressFor(next_), ads1115::REG_CONFIG, ads1115::singleShotConfig(inputFor(next_)));
  convStartMs_ = nowMs;
  if (!converting_) next_ = (next_ + 1) % channelCount();  // skip it this round; it goes stale if it keeps failing
}

bool PowerMonitor::pollConversion(uint32_t nowMs) {
  uint8_t addr = addressFor(next_);
  uint16_t config;
  if (!readRegister(addr, ads1115::REG_CONFIG, config)) return true;  // failed: move on
  if (!ads1115::conversionDone(config)) return (uint32_t)(nowMs - convStartMs_) > cfg::ADS_CONVERSION_TIMEOUT_MS;

  uint16_t raw;
  if (!readRegister(addr, ads1115::REG_CONVERSION, raw)) return true;
  float volts = ads1115::rawToVolts((int16_t)raw);
  if (volts < 0.0f) volts = 0.0f;
  if (next_ == BUS) {
    busV_ = volts * cfg::BUS_DIVIDER;
    busSampled_ = true;
  } else {
    amps_[next_] = volts / cfg::IS_SENSE_OHMS * cfg::IS_RATIO;
    faultFlag_[next_] = volts >= cfg::IS_FAULT_VOLTS;
  }
  lastSampleMs_[next_] = nowMs;
  return true;
}

void PowerMonitor::update() {
  uint32_t mv = 0;
  for (int i = 0; i < 4; i++) mv += analogReadMilliVolts(pins::BATTERY_SENSE);
  batteryV_ = (mv / 4.0f) / 1000.0f * cfg::BATTERY_DIVIDER;

  uint32_t now = millis();
  if (converting_) {
    if (!pollConversion(now)) return;
    next_ = (next_ + 1) % channelCount();
  }
  startConversion(now);
}

float PowerMonitor::driveAmps() const { return amps_[FL] + amps_[FR] + amps_[RL] + amps_[RR]; }

bool PowerMonitor::anyDriverFault() const {
  for (int i = 0; i < currentCount(); i++)
    if (faultFlag_[i]) return true;
  return false;
}

bool PowerMonitor::currentFresh(int ch, uint32_t nowMs) const {
  if (ch >= channelCount()) return false;
  return !readingStale(lastSampleMs_[ch], nowMs, cfg::CURRENT_SENSE_STALE_MS);
}

CurrentSense PowerMonitor::snapshot(uint32_t nowMs) const {
  // cppcheck-suppress knownConditionTrueFalse ; the point is that they are equal
  static_assert(CurrentSense::CURRENTS == CURRENTS && CurrentSense::BUS == BUS, "same channel numbers");
  CurrentSense c = {};
  for (int i = 0; i < CURRENTS; i++) {
    c.amps[i] = amps_[i];
    c.driverFault[i] = faultFlag_[i];
  }
  for (int i = 0; i < CHANNELS; i++) c.fresh[i] = currentFresh(i, nowMs);
  c.driveAmps = driveAmps();
  return c;
}
