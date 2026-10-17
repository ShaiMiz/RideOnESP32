// Lights and sound through the PCA9685 PWM board and the sound module.
// Loads are shed first when the battery runs low. (later: features)
#pragma once

class Accessories {
 public:
  bool begin() { return true; }
  void update(bool /*horn*/, float /*speed*/, bool /*batteryLow*/) {}
};
