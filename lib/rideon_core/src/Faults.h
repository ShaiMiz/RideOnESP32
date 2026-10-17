// Fault codes and the fault latch.
//
// A fault is "active" while its condition is present and "latched" from the
// moment it is first seen until the ModeMachine clears it during re-arming.
// A latched fault never clears itself, so a fault that comes and goes cannot
// restart the motors on its own.
#pragma once

#include <stdint.h>

namespace rideon {

using FaultMask = uint16_t;

enum Fault : FaultMask {
  FAULT_NONE = 0,
  FAULT_LINK_LOST = 1u << 0,       // no RC packet within the timeout, or link quality too low
  FAULT_KILL_SWITCH = 1u << 1,     // radio kill switch on
  FAULT_BATTERY_LOW = 1u << 2,     // below cutoff for longer than the hold time
  FAULT_BATTERY_SENSOR = 1u << 3,  // battery reading impossible: wire off or divider broken
  FAULT_OVERCURRENT = 1u << 4,     // a motor or the total above its limit for longer than the hold time
  FAULT_DRIVER = 1u << 5,          // a BTS7960 IS line pinned high: driver over-temperature or short
  FAULT_INPUT = 1u << 6,           // pedal or switch expander unreadable, or a radio channel out of range
  FAULT_TILT = 1u << 7,            // car tipped past the limit (later, MPU-6050)
  FAULT_SETUP = 1u << 8,           // hardware failed to start; only a reboot clears it
};

// Faults that cut the drivers at once (coast) instead of braking to a stop.
constexpr FaultMask HARD_FAULTS =
    FAULT_KILL_SWITCH | FAULT_OVERCURRENT | FAULT_DRIVER | FAULT_TILT | FAULT_SETUP;

// Faults that also open the main contactor.
constexpr FaultMask CONTACTOR_FAULTS = FAULT_KILL_SWITCH | FAULT_SETUP;

// Faults that a re-arm can never clear.
constexpr FaultMask PERMANENT_FAULTS = FAULT_SETUP;

class FaultLatch {
 public:
  // Report this loop's active conditions. Every active fault becomes latched.
  void update(FaultMask active) {
    active_ = active;
    latched_ |= active;
  }

  // Latch a fault without an ongoing condition (for example a setup failure).
  void raise(FaultMask f) { latched_ |= f; }

  // Clear latched faults whose condition is gone, except permanent ones.
  // Returns true when nothing is left latched.
  bool clearInactive() {
    latched_ &= (active_ | PERMANENT_FAULTS);
    return latched_ == FAULT_NONE;
  }

  FaultMask active() const { return active_; }
  FaultMask latched() const { return latched_; }
  bool any() const { return latched_ != FAULT_NONE; }
  bool has(FaultMask f) const { return (latched_ & f) != 0; }

 private:
  FaultMask active_ = FAULT_NONE;
  FaultMask latched_ = FAULT_NONE;
};

// True once `condition` has held continuously for `holdMs`.
// Used so a brief spike (motor inrush, battery sag) does not trip a fault.
class HoldTimer {
 public:
  bool update(bool condition, uint32_t nowMs, uint32_t holdMs) {
    if (!condition) {
      running_ = false;
      return false;
    }
    if (!running_) {
      running_ = true;
      startMs_ = nowMs;
    }
    return (uint32_t)(nowMs - startMs_) >= holdMs;
  }

 private:
  bool running_ = false;
  uint32_t startMs_ = 0;
};

// Short name for logs, the radio screen and the OLED. Returns the first
// (lowest) fault in the mask.
const char* faultName(FaultMask mask);

}  // namespace rideon
