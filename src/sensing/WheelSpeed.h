// Hall-sensor wheel speed on GPIO34 / GPIO35 (not 36/39: they glitch when the ADC runs). (later: traction control)
#pragma once

class WheelSpeed {
 public:
  bool begin() { return true; }
  void update() {}
  float kmh() const { return 0.0f; }
};
