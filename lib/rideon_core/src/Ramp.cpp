#include "Ramp.h"

namespace rideon {

static float absf(float v) { return v < 0 ? -v : v; }

float Ramp::step(float target, float dtS, const RampParams& p) {
  if (target > 1.0f) target = 1.0f;
  if (target < -1.0f) target = -1.0f;

  // Opposite direction requested while still moving: brake to zero first.
  if ((current_ > p.stoppedEps && target < 0.0f) || (current_ < -p.stoppedEps && target > 0.0f)) {
    target = 0.0f;
  }

  bool speedingUp = absf(target) > absf(current_) && target * current_ >= 0.0f;
  float maxStep = (speedingUp ? p.accelPerS : p.brakePerS) * dtS;
  float diff = target - current_;
  if (diff > maxStep) diff = maxStep;
  if (diff < -maxStep) diff = -maxStep;
  current_ += diff;

  if (target == 0.0f && absf(current_) < p.stoppedEps) current_ = 0.0f;
  return current_;
}

bool Ramp::stopped(const RampParams& p) const { return absf(current_) < p.stoppedEps; }

float limitCommand(float cmd, float cap, float reverseCap) {
  if (cmd > 1.0f) cmd = 1.0f;
  if (cmd < -1.0f) cmd = -1.0f;
  return cmd >= 0.0f ? cmd * cap : cmd * cap * reverseCap;
}

}  // namespace rideon
