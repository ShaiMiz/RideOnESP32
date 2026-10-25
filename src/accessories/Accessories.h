// Box fans, lights and sound through the PCA9685 PWM board and the sound
// module.
//
// Fans: PCA9685 channel 1 drives U29 IN2, the low side of both box fans. They
// run at full speed whenever the ESP32 runs, and the command is re-sent every
// FAN_REFRESH_MS, so a PCA9685 that reset (it wakes up asleep, outputs off)
// gets them going again. A temperature-controlled speed can come later; full
// speed is the safe default.
//
// Lights and sound: later (features). Loads are shed first when the battery
// runs low.
#pragma once

#include <stdint.h>

class Accessories {
 public:
  // Returns false if the PCA9685 does not answer (the fans then stay off).
  bool begin();
  void update(bool horn, float speed, bool batteryLow);

  bool fansOn() const { return fansOn_; }

 private:
  bool commandFans();

  bool fansOn_ = false;
  uint32_t lastFanMs_ = 0;
};
