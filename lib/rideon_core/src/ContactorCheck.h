// Checks the main contactor (K1) against the motor-bus voltage.
//
// The motor bus (MOTOR_24V, after K1) is read through a 150k / 12k divider on
// ADS1115 #2 A2 and compared with the battery reading, so both checks work at
// any state of charge. A 2.2k bleed resistor drains the driver capacitors
// while K1 is open.
//
// Boot weld check: K1 may close only after the bus has drained below
// openMaxFraction of the battery. If it is still up after dischargeTimeoutMs,
// K1 is welded shut (or something feeds the bus around it), and the E-stop no
// longer isolates the drivers. That is permanent until a reboot repeats the
// check. The check runs only at boot: a car pushed with K1 open can charge
// the bus through the motors and would look welded.
//
// Missing bus: K1 commanded closed for closeTimeMs but the bus below
// closedMinFraction of the battery. Coil fuse, coil wiring, an E-stop pressed
// with a broken sense wire, or a K1 that no longer closes.
#pragma once

#include <stdint.h>

#include "Faults.h"

namespace rideon {

struct ContactorParams {
  float openMaxFraction;       // bus below this share of the battery: K1 open and the caps drained
  float closedMinFraction;     // bus above this share of the battery: K1 closed
  uint32_t dischargeTimeoutMs; // boot: the bus must drain within this, or K1 counts as welded
  uint32_t closeTimeMs;        // K1 commanded closed: the bus must be up within this
};

enum class ContactorState : uint8_t { WaitingForDischarge, Ready, Welded };

class ContactorCheck {
 public:
  // Call every loop. readingsValid: bus and battery readings are fresh and
  // plausible; without them nothing passes or fails. commandedClosed: what
  // CONTACTOR_HOLD was set to last loop.
  void update(bool readingsValid, float busVolts, float batteryVolts, bool commandedClosed, uint32_t nowMs,
              const ContactorParams& p);

  // K1 may be closed only after the boot weld check passed.
  bool allowClose() const { return state_ == ContactorState::Ready; }
  bool welded() const { return state_ == ContactorState::Welded; }
  // No motor bus while it should be there: still waiting for the boot check,
  // or K1 commanded closed and the bus did not come up.
  bool busMissing() const { return state_ == ContactorState::WaitingForDischarge || missing_; }

  ContactorState state() const { return state_; }

 private:
  ContactorState state_ = ContactorState::WaitingForDischarge;
  bool started_ = false;
  uint32_t startMs_ = 0;
  bool missing_ = false;
  HoldTimer low_;
};

}  // namespace rideon
