// Battery voltage, motor-bus voltage and motor current.
//
// Battery: GPIO39 (VN) through a 150k/12k divider, read with analogReadMilliVolts()
// (calibrated), about 2.2 V at 29.4 V, inside the ESP32's accurate range.
//
// Motor bus: MOTOR_24V (after the contactor) through another 150k/12k divider
// on ADS1115 #2 A2, for the contactor checks (rideon::ContactorCheck).
//
// Current: each BTS7960 board's R_IS and L_IS are joined into one sense line,
// fed through 4.7k and a 3.3 V clamp into an ADS1115. Load current is
// IS volts / IS_SENSE_OHMS * 8500, where IS_SENSE_OHMS is the board's two IS
// resistors in parallel. An IS line above IS_FAULT_VOLTS means the driver is
// signalling a fault (it sources 4-7 mA). One channel is converted per call,
// so the loop never waits on the ADC; each channel refreshes at LOOP_HZ / 6.
//
// Every I2C transfer is checked. A channel whose reading stops updating (bus
// stuck, ADS1115 unplugged) shows up in currentFresh(), so the safety layer
// can stop the car instead of trusting the last value.
#pragma once

#include <stdint.h>

class PowerMonitor {
 public:
  static constexpr int CURRENTS = 5;
  static constexpr int CHANNELS = 6;
  enum Channel { FL, FR, RL, RR, STEER, BUS };

  // Returns false if an ADS1115 does not answer. Pass driveOnly to use only
  // the first ADS1115 (bench firmware: no steering current, no bus).
  bool begin(bool driveOnly = false);
  void update();

  float batteryVolts() const { return batteryV_; }
  float amps(int ch) const { return amps_[ch]; }
  float driveAmps() const;  // sum of the four wheels
  bool driverFault(int ch) const { return faultFlag_[ch]; }
  bool anyDriverFault() const;
  float busVolts() const { return busV_; }
  // True once the bus has been read for real and the reading is fresh. Not
  // just currentFresh(BUS): that is also true for the grace period at boot,
  // when busVolts() is still 0 and would pass the weld check.
  bool busReadingValid(uint32_t nowMs) const { return busSampled_ && currentFresh(BUS, nowMs); }

  // True while channel `ch` (BUS included) has had a good reading within cfg::CURRENT_SENSE_STALE_MS.
  bool currentFresh(int ch, uint32_t nowMs) const;

 private:
  int channelCount() const { return driveOnly_ ? 4 : CHANNELS; }
  int currentCount() const { return driveOnly_ ? 4 : CURRENTS; }
  void startConversion(uint32_t nowMs);
  // Polls the running conversion. Returns false while it is still busy.
  bool pollConversion(uint32_t nowMs);

  bool driveOnly_ = false;
  int next_ = 0;
  bool converting_ = false;
  uint32_t convStartMs_ = 0;
  float batteryV_ = 0.0f;
  float busV_ = 0.0f;
  bool busSampled_ = false;
  float amps_[CURRENTS] = {};
  bool faultFlag_[CURRENTS] = {};
  uint32_t lastSampleMs_[CHANNELS] = {};
};
