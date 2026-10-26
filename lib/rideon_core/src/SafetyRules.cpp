#include "SafetyRules.h"

namespace rideon {

namespace {
Component wheelComponent(int i) {
  static const Component WHEELS[] = {Component::FL, Component::FR, Component::RL, Component::RR};
  return i >= 0 && i < 4 ? WHEELS[i] : Component::None;
}
}  // namespace

FaultMask SafetyRules::evaluate(const SafetyInputs& s, const SafetyLimits& lim) {
  // Every fault is filed as a report naming the part, for the log and the
  // fault table; the mask the reports add up to is what stops the car.
  FaultReports& r = reports_;
  r.clear();

  if (s.link == LinkState::Lost) r.add(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  if (s.killSwitch) r.add(FAULT_KILL_SWITCH, Component::Radio, Symptom::Active);
  if (!s.radioChannelsValid) r.add(FAULT_INPUT, Component::Radio, Symptom::OutOfRange);
  if (s.kidRequested && !s.manualOk) r.add(FAULT_INPUT, Component::Pcf, Symptom::NoAnswer);
  if (s.estop) r.add(FAULT_ESTOP, Component::Estop, Symptom::Active);

  switch (battery_.update(s.batteryVolts, s.nowMs, lim.battery)) {
    case BatteryState::SensorFault:
      r.add(FAULT_BATTERY_SENSOR, Component::Battery, Symptom::OutOfRange, s.batteryVolts);
      break;
    case BatteryState::Cutoff:
      r.add(FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit, s.batteryVolts, lim.battery.cutoffV);
      break;
    default: break;
  }

  if (const CurrentSense* c = s.current) {
    // Without fresh readings the over-current and driver checks below would
    // be judging old values, so a lost ADS1115 is a fault of its own.
    for (int i = 0; i < s.wheelsSensed; i++)
      if (!c->fresh[i]) r.add(FAULT_CURRENT_SENSE, wheelComponent(i), Symptom::Stale);
    if (s.steerSensed && !c->fresh[CurrentSense::STEER]) r.add(FAULT_CURRENT_SENSE, Component::Steer, Symptom::Stale);
    if (s.busSensed && !c->fresh[CurrentSense::BUS]) r.add(FAULT_CURRENT_SENSE, Component::Bus, Symptom::Stale);

    for (int i = 0; i < s.wheelsSensed; i++) {
      if (overcurrent_[i].update(c->amps[i] > lim.motorCurrentA, s.nowMs, lim.overcurrentHoldMs))
        r.add(FAULT_OVERCURRENT, wheelComponent(i), Symptom::OverLimit, c->amps[i], lim.motorCurrentA);
      if (driverFault_[i].update(c->driverFault[i], s.nowMs, lim.driverFaultHoldMs))
        r.add(FAULT_DRIVER, wheelComponent(i), Symptom::StuckHigh);
    }
    if (s.steerSensed &&
        driverFault_[CurrentSense::STEER].update(c->driverFault[CurrentSense::STEER], s.nowMs, lim.driverFaultHoldMs))
      r.add(FAULT_DRIVER, Component::Steer, Symptom::StuckHigh);
    if (totalOvercurrent_.update(c->driveAmps > lim.totalCurrentA, s.nowMs, lim.overcurrentHoldMs))
      r.add(FAULT_OVERCURRENT, Component::Wheels, Symptom::OverLimit, c->driveAmps, lim.totalCurrentA);
  }

  if (s.contactorChecked) {
    if (s.busMissing) r.add(FAULT_NO_BUS, Component::K1, s.busWaiting ? Symptom::Waiting : Symptom::Missing);
    if (s.k1Welded) r.add(FAULT_K1_WELDED, Component::K1, Symptom::Welded);
  }
  if (s.boxHot)
    r.add(FAULT_OVERTEMP, Component::Box, s.boxSensorLost ? Symptom::Stale : Symptom::OverLimit, s.boxCelsius,
          lim.boxLimitC);
  if (s.tilted) r.add(FAULT_TILT, Component::Imu, Symptom::OverLimit);
  return r.mask();
}

KillLineState killLines(Mode mode, const FaultLatch& faults, bool driveStopped, bool contactorAllowed) {
  bool cut = mode == Mode::Killed || faults.has(HARD_FAULTS);
  bool driving = mode == Mode::Kid || mode == Mode::Parent;
  return {!cut && (driving || !driveStopped), contactorAllowed && !faults.has(CONTACTOR_FAULTS), cut};
}

}  // namespace rideon
