#include "ContactorCheck.h"

namespace rideon {

void ContactorCheck::update(bool readingsValid, float busVolts, float batteryVolts, bool commandedClosed,
                            uint32_t nowMs, const ContactorParams& p) {
  if (!started_) {
    started_ = true;
    startMs_ = nowMs;
  }

  if (state_ == ContactorState::WaitingForDischarge && readingsValid && !commandedClosed) {
    if (busVolts < p.openMaxFraction * batteryVolts) {
      state_ = ContactorState::Ready;
    } else if ((uint32_t)(nowMs - startMs_) >= p.dischargeTimeoutMs) {
      state_ = ContactorState::Welded;
    }
  }

  bool low = state_ == ContactorState::Ready && commandedClosed && readingsValid &&
             busVolts < p.closedMinFraction * batteryVolts;
  missing_ = low_.update(low, nowMs, p.closeTimeMs);
}

}  // namespace rideon
