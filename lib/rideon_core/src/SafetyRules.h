// The safety rules: every sensor reading in, the active fault conditions
// out, and how the kill lines must be set for the mode and the latched
// faults. No hardware calls, so every rule is unit-tested on the PC
// (test/test_safety_rules, test/test_controller).
//
// Kill-line rules:
//   drive EN   LOW at once on a latched HARD_FAULT or in Killed (motors
//              coast); HIGH in Kid and Parent; in Locked, HIGH only while
//              the car brakes to a stop, then LOW
//   contactor  open while a CONTACTOR_FAULT is latched (kill switch, E-stop,
//              setup, driver, over-current, weld) and until the boot weld
//              check has passed. Set after the enable, so on a driver or
//              over-current fault the drivers are off before K1 opens.
#pragma once

#include <stdint.h>

#include "BatteryCheck.h"
#include "FaultReport.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

namespace rideon {

// One loop's current-sense readings (PowerMonitor in the car firmware).
struct CurrentSense {
  static constexpr int CURRENTS = 5;  // FL, FR, RL, RR, steering
  static constexpr int STEER = 4;
  static constexpr int BUS = 5;       // fresh[] only: the motor-bus reading
  float amps[CURRENTS];
  bool driverFault[CURRENTS];  // IS line above the driver-fault level
  bool fresh[CURRENTS + 1];    // a good reading within the stale time
  float driveAmps;             // the four wheels together
};

struct SafetyLimits {
  BatteryParams battery;
  float motorCurrentA;      // one wheel
  float totalCurrentA;      // four wheels together
  uint32_t overcurrentHoldMs;
  uint32_t driverFaultHoldMs;
  float boxLimitC;          // for the report only; the box check decides
};

struct SafetyInputs {
  LinkState link;
  bool killSwitch;
  bool radioChannelsValid;  // throttle, steering, kill channels in range
  bool manualOk;            // pedal/switch expander answering
  bool estop;               // E-stop sense input reads pressed
  bool kidRequested;        // mode switch at Kid
  float batteryVolts;
  const CurrentSense* current;  // nullptr: no current sensing
  int wheelsSensed;             // drive channels to check (bench: 1)
  bool steerSensed;             // also check the steering driver's IS line
  bool busSensed;               // also check the motor-bus reading is fresh
  bool contactorChecked;        // the contactor flags below are real
  bool busMissing;              // ContactorCheck::busMissing()
  bool busWaiting;              // ... because the boot weld check is still running
  bool k1Welded;
  bool boxHot;
  bool boxSensorLost;  // HOT because the reading is lost, not because the box is hot
  float boxCelsius;
  bool tilted;
  uint32_t nowMs;
};

struct KillLineState {
  bool drive;      // DRIVE_EN
  bool contactor;  // CONTACTOR_HOLD
  bool cut;        // stop the drive at once: zero the outputs, no ramp
};

class SafetyRules {
 public:
  // Active fault conditions this loop. reports() then says which part each came from.
  FaultMask evaluate(const SafetyInputs& in, const SafetyLimits& lim);
  const FaultReports& reports() const { return reports_; }
  BatteryState battery() const { return battery_.state(); }
  bool batteryHigh() const { return battery_.highWarning(); }

 private:
  BatteryCheck battery_;
  HoldTimer overcurrent_[CurrentSense::CURRENTS];
  HoldTimer driverFault_[CurrentSense::CURRENTS];
  HoldTimer totalOvercurrent_;
  FaultReports reports_;
};

// driveStopped: the ramp output is at zero. contactorAllowed: the boot weld check passed.
KillLineState killLines(Mode mode, const FaultLatch& faults, bool driveStopped, bool contactorAllowed);

}  // namespace rideon
