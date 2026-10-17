// Gathers every safety signal once per loop, reports the active fault
// conditions, and sets the kill lines for the current mode.
//
// Output rules:
//   contactor  open while a CONTACTOR_FAULT is latched (kill switch, E-stop, setup)
//   drive EN   LOW at once on a latched HARD_FAULT (motors coast);
//              HIGH in Kid and Parent; in Locked, HIGH only while the car
//              brakes to a stop, then LOW
#pragma once

#include <stdint.h>

#include "BatteryCheck.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

class KillLines;
class PowerMonitor;

struct SafetySignals {
  rideon::LinkState link;
  bool killSwitch;
  bool radioChannelsValid;  // throttle, steering, kill channels in range
  bool manualOk;            // pedal/switch expander answering
  bool estop;               // E-stop sense input reads pressed
  bool kidRequested;        // mode switch at Kid
  float batteryVolts;
  const PowerMonitor* power;  // nullptr: no current sensing
  int wheelsSensed;           // how many drive channels to check (bench: 1)
  bool tilted;
  uint32_t nowMs;
};

class SafetyManager {
 public:
  // Active fault conditions this loop.
  rideon::FaultMask evaluate(const SafetySignals& s);

  // Set the kill lines. Returns true if the drive must stop at once.
  bool apply(rideon::Mode mode, const rideon::FaultLatch& faults, bool driveStopped, KillLines& lines);

  rideon::BatteryState battery() const { return battery_.state(); }
  bool batteryHigh() const { return battery_.highWarning(); }

 private:
  rideon::BatteryCheck battery_;
  rideon::HoldTimer overcurrent_[5];
  rideon::HoldTimer totalOvercurrent_;
};
