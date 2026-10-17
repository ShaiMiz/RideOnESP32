#include "ArmingCheck.h"

namespace rideon {

static bool nearCenter(uint16_t us, uint16_t center, uint16_t tol) {
  int d = (int)us - (int)center;
  return d >= -(int)tol && d <= (int)tol;
}

bool armingOk(const ArmingInputs& in, const ArmingParams& p) {
  return nearCenter(in.throttleUs, p.center, p.toleranceUs) &&
         nearCenter(in.steeringUs, p.center, p.toleranceUs) &&
         in.pedal < p.pedalReleased;
}

}  // namespace rideon
