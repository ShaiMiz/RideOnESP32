#include "PowerMonitor.h"

#include <Arduino.h>

#include "config.h"
#include "pins.h"

namespace {
const uint16_t MUX[4] = {ADS1X15_REG_CONFIG_MUX_SINGLE_0, ADS1X15_REG_CONFIG_MUX_SINGLE_1,
                         ADS1X15_REG_CONFIG_MUX_SINGLE_2, ADS1X15_REG_CONFIG_MUX_SINGLE_3};
}

bool PowerMonitor::begin(bool driveOnly) {
  driveOnly_ = driveOnly;
  pinMode(pins::BATTERY_SENSE, INPUT);
  analogSetPinAttenuation(pins::BATTERY_SENSE, ADC_11db);

  bool ok = adsDrive_.begin(i2c::ADS_DRIVE);
  if (!driveOnly_) ok &= adsAux_.begin(i2c::ADS_AUX);
  for (Adafruit_ADS1115* a : {&adsDrive_, &adsAux_}) {
    a->setGain(GAIN_ONE);  // +/-4.096 V full scale, covers the 3.3 V clamp
    a->setDataRate(RATE_ADS1115_860SPS);
  }
  if (ok) startConversion();
  return ok;
}

void PowerMonitor::startConversion() {
  Adafruit_ADS1115& ads = next_ < 4 ? adsDrive_ : adsAux_;
  ads.startADCReading(MUX[next_ % 4], false);
  converting_ = true;
}

void PowerMonitor::update() {
  uint32_t mv = 0;
  for (int i = 0; i < 4; i++) mv += analogReadMilliVolts(pins::BATTERY_SENSE);
  batteryV_ = (mv / 4.0f) / 1000.0f * cfg::BATTERY_DIVIDER;

  if (!converting_) return;
  Adafruit_ADS1115& ads = next_ < 4 ? adsDrive_ : adsAux_;
  if (!ads.conversionComplete()) return;
  float volts = ads.computeVolts(ads.getLastConversionResults());
  if (volts < 0.0f) volts = 0.0f;
  amps_[next_] = volts / cfg::IS_RESISTOR_OHMS * cfg::IS_RATIO;
  faultFlag_[next_] = volts >= cfg::IS_FAULT_VOLTS;

  int channels = driveOnly_ ? 4 : CHANNELS;
  next_ = (next_ + 1) % channels;
  startConversion();
}

float PowerMonitor::driveAmps() const { return amps_[FL] + amps_[FR] + amps_[RL] + amps_[RR]; }

bool PowerMonitor::anyDriverFault() const {
  for (int i = 0; i < (driveOnly_ ? 4 : CHANNELS); i++)
    if (faultFlag_[i]) return true;
  return false;
}
