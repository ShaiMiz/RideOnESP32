#include "BatteryCheck.h"

namespace rideon {

BatteryState BatteryCheck::update(float volts, uint32_t nowMs, const BatteryParams& p) {
  if (volts < p.sensorMinV || volts > p.sensorMaxV) {
    below_.update(false, nowMs, p.cutoffHoldMs);
    state_ = BatteryState::SensorFault;
    return state_;
  }
  high_ = volts > p.highWarnV;

  if (below_.update(volts < p.cutoffV, nowMs, p.cutoffHoldMs)) cutoff_ = true;
  if (cutoff_ && volts > p.recoverV) cutoff_ = false;

  if (cutoff_) state_ = BatteryState::Cutoff;
  else if (volts < p.lowWarnV) state_ = BatteryState::Low;
  else state_ = BatteryState::Ok;
  return state_;
}

}  // namespace rideon
