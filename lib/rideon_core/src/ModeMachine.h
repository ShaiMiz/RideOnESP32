// Who is allowed to drive: Locked, Kid, Parent or Killed.
//
// Rules (docs/firmware/CONTROL_MODES.md):
// - Kill switch on: Killed, whatever else is happening.
// - Any latched fault, or an invalid mode switch: Locked.
// - Leaving Locked needs: no latched faults, the mode switch seen in the
//   Locked position since the last stop, and the arming check passing.
// - Faults are cleared only from Locked, with the mode switch at Locked and
//   the arming check passing, and only if their condition is gone.
// - Kid <-> Parent directly only when the arming check passes; otherwise the
//   car goes to Locked until it does.
#pragma once

#include <stdint.h>

#include "Faults.h"

namespace rideon {

enum class Mode : uint8_t { Locked, Kid, Parent, Killed };
enum class ModeRequest : uint8_t { Locked, Kid, Parent, Invalid };

struct ModeInputs {
  bool killSwitch;      // true = kill switch on
  ModeRequest request;  // from the radio's 3-position switch
  bool armingOk;        // from armingOk()
};

// Map a switchPosition() result to a request.
inline ModeRequest modeRequestFromSwitch(int position) {
  switch (position) {
    case 0: return ModeRequest::Locked;
    case 1: return ModeRequest::Kid;
    case 2: return ModeRequest::Parent;
    default: return ModeRequest::Invalid;
  }
}

class ModeMachine {
 public:
  Mode update(const ModeInputs& in, FaultLatch& faults);

  Mode mode() const { return mode_; }
  // True while the mode switch must be put to Locked before driving again.
  bool needsSwitchCycle() const { return needCycle_; }

 private:
  Mode mode_ = Mode::Locked;
  bool needCycle_ = true;  // at boot the switch must be seen at Locked first
};

const char* modeName(Mode m);

}  // namespace rideon
