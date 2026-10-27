// One control tick of the car, with no hardware calls: radio channels,
// pedal and sensor readings in; mode, speed and kill lines out.
//
// The car firmware (src/main.cpp) only reads the hardware into
// ControlInputs, calls step() at LOOP_HZ, and writes ControlOutputs to the
// drivers and kill lines. Everything that decides whether the car may move
// is here, so the whole decision path runs in the PC tests with the real
// config.h values (test/test_controller, docs/VERIFICATION.md).
//
// Order inside a tick:
//   1. judge the link (never by channel values: they freeze when it drops)
//   2. decode the channels, trusted only while the link is up
//   3. evaluate the safety rules and latch the faults
//   4. update the mode (re-arming clears faults whose condition is gone)
//   5. pick the command for the mode, with its caps
//   6. set the kill lines from the mode, the latched faults and whether the
//      car has stopped
//   7. ramp toward the command, or zero at once when the drive is cut
#pragma once

#include <stdint.h>

#include "ArmingCheck.h"
#include "ChannelMap.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"
#include "Ramp.h"
#include "SafetyRules.h"

namespace rideon {

struct ControlConfig {
  ChannelLimits channels;
  LinkParams link;
  ArmingParams arming;
  RampParams parentRamp;
  RampParams kidRamp;
  SafetyLimits safety;
  float kidCaps[3];        // speed-cap switch low, middle, high
  float parentCap;
  float reverseCap;        // reverse is capped to this fraction of the forward cap
  float degradedCap;       // extra cap while the link is degraded
  float kidBrakeOverride;  // throttle stick at or below this in Kid mode: brake, pedal ignored
  int wheelsSensed;        // drive channels with current sense (car 4, bench 1)
  bool steerSensed;
  bool busSensed;
};

struct ControlInputs {
  LinkInputs link;
  uint16_t throttleUs, steeringUs, killUs, modeUs, speedCapUs, hornUs;
  bool manualOk;  // pedal/switch expander answering
  bool estopPressed;
  float pedal;        // 0..1
  int8_t direction;   // +1 forward, -1 reverse, 0 neutral
  bool kidHorn;
  float batteryVolts;
  const CurrentSense* current;  // nullptr: no current sensing
  bool contactorChecked;        // the contactor flags below are real
  bool busMissing;
  bool busWaiting;
  bool k1Welded;
  bool contactorAllowed;  // the boot weld check has passed
  bool boxHot;
  bool boxSensorLost;
  float boxCelsius;
  bool tilted;
  uint32_t nowMs;
  float dtS;
};

struct ControlOutputs {
  Mode mode;
  LinkState link;
  float command;   // what the mode asks for, capped (-1..1)
  float speed;     // ramped output to write to every wheel (-1..1)
  float steering;  // steering stick, -1..1 (0 while the link is down)
  KillLineState lines;
  bool horn;
};

class Controller {
 public:
  explicit Controller(const ControlConfig& cfg) : cfg_(cfg) {}

  ControlOutputs step(const ControlInputs& in);

  // Latch a fault with no ongoing condition (a part that failed at setup).
  void raise(FaultMask f) { faults_.raise(f); }

  const FaultLatch& faults() const { return faults_; }
  const FaultReports& reports() const { return rules_.reports(); }
  BatteryState battery() const { return rules_.battery(); }
  Mode mode() const { return modes_.mode(); }
  float speed() const { return ramp_.value(); }
  bool stopped() const { return ramp_.stopped(rampParams()); }

 private:
  float kidCap(uint16_t speedCapUs) const;
  const RampParams& rampParams() const { return kidRamp_ ? cfg_.kidRamp : cfg_.parentRamp; }

  ControlConfig cfg_;
  SafetyRules rules_;
  FaultLatch faults_;
  ModeMachine modes_;
  Ramp ramp_;
  bool kidRamp_ = false;  // Kid mode's gentler acceleration
};

}  // namespace rideon
