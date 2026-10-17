// Battery voltage and motor current.
//
// Battery: GPIO39 (VN) through a 150k/12k divider, read with analogReadMilliVolts()
// (calibrated), about 2.2 V at 29.4 V, inside the ESP32's accurate range.
//
// Current: each BTS7960 board's R_IS and L_IS are joined into one sense line,
// fed through 4.7k and a 3.3 V clamp into an ADS1115. Load current is
// IS volts / IS resistor * 8500. An IS line pinned at the clamp means the
// driver is signalling a fault (it sources 4-7 mA). One channel is converted
// per call, so the loop never waits on the ADC; each channel refreshes at
// LOOP_HZ / 5.
//
// Every I2C transfer is checked. A channel whose reading stops updating (bus
// stuck, ADS1115 unplugged) shows up in currentFresh(), so the safety layer
// can stop the car instead of trusting the last value.
#pragma once

#include <stdint.h>

class PowerMonitor {
 public:
  static constexpr int CHANNELS = 5;
  enum Channel { FL, FR, RL, RR, STEER };

  // Returns false if an ADS1115 does not answer. Pass driveOnly to use only
  // the first ADS1115 (bench firmware).
  bool begin(bool driveOnly = false);
  void update();

  float batteryVolts() const { return batteryV_; }
  float amps(int ch) const { return amps_[ch]; }
  float driveAmps() const;  // sum of the four wheels
  bool driverFault(int ch) const { return faultFlag_[ch]; }
  bool anyDriverFault() const;

  // True while channel `ch` has had a good reading within cfg::CURRENT_SENSE_STALE_MS.
  bool currentFresh(int ch, uint32_t nowMs) const;

 private:
  int channelCount() const { return driveOnly_ ? 4 : CHANNELS; }
  void startConversion(uint32_t nowMs);
  // Polls the running conversion. Returns false while it is still busy.
  bool pollConversion(uint32_t nowMs);

  bool driveOnly_ = false;
  int next_ = 0;
  bool converting_ = false;
  uint32_t convStartMs_ = 0;
  float batteryV_ = 0.0f;
  float amps_[CHANNELS] = {};
  bool faultFlag_[CHANNELS] = {};
  uint32_t lastSampleMs_[CHANNELS] = {};
};
