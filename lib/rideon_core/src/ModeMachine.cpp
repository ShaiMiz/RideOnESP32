#include "ModeMachine.h"

namespace rideon {

Mode ModeMachine::update(const ModeInputs& in, FaultLatch& faults) {
  if (in.killSwitch) {
    mode_ = Mode::Killed;
    needCycle_ = true;
    return mode_;
  }
  if (mode_ == Mode::Killed) {
    mode_ = Mode::Locked;  // kill released: stop in Locked, never straight back to driving
  }

  if (in.request == ModeRequest::Invalid) {
    mode_ = Mode::Locked;
    needCycle_ = true;
    return mode_;
  }

  if (faults.any()) {
    if (mode_ != Mode::Locked) {
      mode_ = Mode::Locked;
      needCycle_ = true;
      return mode_;
    }
    if (in.request == ModeRequest::Locked && in.armingOk) {
      faults.clearInactive();
    }
    if (faults.any()) return mode_;
  }

  if (in.request == ModeRequest::Locked) {
    needCycle_ = false;
    mode_ = Mode::Locked;
    return mode_;
  }

  Mode wanted = in.request == ModeRequest::Kid ? Mode::Kid : Mode::Parent;
  if (mode_ == Mode::Locked) {
    if (!needCycle_ && in.armingOk) mode_ = wanted;
  } else if (mode_ != wanted) {
    mode_ = in.armingOk ? wanted : Mode::Locked;
  }
  return mode_;
}

const char* modeName(Mode m) {
  switch (m) {
    case Mode::Locked: return "LOCKED";
    case Mode::Kid: return "KID";
    case Mode::Parent: return "PARENT";
    case Mode::Killed: return "KILLED";
  }
  return "?";
}

}  // namespace rideon
