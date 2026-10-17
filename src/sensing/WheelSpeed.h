// Hall-sensor wheel speed on GPIO36 / GPIO39. (later: traction control)
#pragma once

class WheelSpeed {
 public:
  bool begin() { return true; }
  void update() {}
  float kmh() const { return 0.0f; }
};
