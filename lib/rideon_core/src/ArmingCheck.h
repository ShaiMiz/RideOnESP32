// The conditions for arming, re-arming after a fault, or changing mode:
// both sticks near centre and the pedal released. This stops the car from
// surging when it is armed while something is held down.
#pragma once

#include <stdint.h>

namespace rideon {

struct ArmingInputs {
  uint16_t throttleUs;
  uint16_t steeringUs;
  float pedal;  // 0..1
};

struct ArmingParams {
  uint16_t center;
  uint16_t toleranceUs;
  float pedalReleased;  // pedal must be below this
};

bool armingOk(const ArmingInputs& in, const ArmingParams& p);

}  // namespace rideon
