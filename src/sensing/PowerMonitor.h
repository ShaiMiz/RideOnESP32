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
#pragma once

#include <Adafruit_ADS1X15.h>

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

 private:
  void startConversion();

  Adafruit_ADS1115 adsDrive_;
  Adafruit_ADS1115 adsAux_;
  bool driveOnly_ = false;
  int next_ = 0;
  bool converting_ = false;
  float batteryV_ = 0.0f;
  float amps_[CHANNELS] = {};
  bool faultFlag_[CHANNELS] = {};
};
