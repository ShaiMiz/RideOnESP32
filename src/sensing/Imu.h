// MPU-6050 tilt and crash cut-out. (later: features)
// Will raise FAULT_TILT past a set roll/pitch angle or a hard impact.
#pragma once

class Imu {
 public:
  bool begin() { return true; }
  void update() {}
  bool tilted() const { return false; }
};
