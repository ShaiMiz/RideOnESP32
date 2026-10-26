#include "SafetyManager.h"

#include "config.h"
#include "safety/KillLines.h"
#include "sensing/PowerMonitor.h"

using namespace rideon;

namespace {
Component wheelComponent(int i) {
  static const Component WHEELS[] = {Component::FL, Component::FR, Component::RL, Component::RR};
  return i >= 0 && i < 4 ? WHEELS[i] : Component::None;
}

const BatteryParams BATTERY = {cfg::BATTERY_SENSOR_MIN_V, cfg::BATTERY_SENSOR_MAX_V, cfg::BATTERY_LOW_WARN_V,
                               cfg::BATTERY_CUTOFF_V,     cfg::BATTERY_RECOVER_V,    cfg::BATTERY_HIGH_WARN_V,
                               cfg::BATTERY_CUTOFF_HOLD_MS};
}

FaultMask SafetyManager::evaluate(const SafetySignals& s) {
  // Every fault is filed as a report naming the part, for the log and the
  // fault table; the mask the reports add up to is what stops the car.
  FaultReports& r = reports_;
  r.clear();

  if (s.link == LinkState::Lost) r.add(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  if (s.killSwitch) r.add(FAULT_KILL_SWITCH, Component::Radio, Symptom::Active);
  if (!s.radioChannelsValid) r.add(FAULT_INPUT, Component::Radio, Symptom::OutOfRange);
  if (s.kidRequested && !s.manualOk) r.add(FAULT_INPUT, Component::Pcf, Symptom::NoAnswer);
  if (s.estop) r.add(FAULT_ESTOP, Component::Estop, Symptom::Active);

  switch (battery_.update(s.batteryVolts, s.nowMs, BATTERY)) {
    case BatteryState::SensorFault:
      r.add(FAULT_BATTERY_SENSOR, Component::Battery, Symptom::OutOfRange, s.batteryVolts);
      break;
    case BatteryState::Cutoff:
      r.add(FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit, s.batteryVolts, cfg::BATTERY_CUTOFF_V);
      break;
    default: break;
  }

  if (s.power) {
    // Without fresh readings the over-current and driver checks below would
    // be judging old values, so a lost ADS1115 is a fault of its own.
    for (int i = 0; i < s.wheelsSensed; i++)
      if (!s.power->currentFresh(i, s.nowMs)) r.add(FAULT_CURRENT_SENSE, wheelComponent(i), Symptom::Stale);
    if (s.steerSensed && !s.power->currentFresh(PowerMonitor::STEER, s.nowMs))
      r.add(FAULT_CURRENT_SENSE, Component::Steer, Symptom::Stale);
    if (s.busSensed && !s.power->currentFresh(PowerMonitor::BUS, s.nowMs))
      r.add(FAULT_CURRENT_SENSE, Component::Bus, Symptom::Stale);

    for (int i = 0; i < s.wheelsSensed; i++) {
      float amps = s.power->amps(i);
      if (overcurrent_[i].update(amps > cfg::MOTOR_CURRENT_LIMIT_A, s.nowMs, cfg::OVERCURRENT_HOLD_MS))
        r.add(FAULT_OVERCURRENT, wheelComponent(i), Symptom::OverLimit, amps, cfg::MOTOR_CURRENT_LIMIT_A);
      if (driverFault_[i].update(s.power->driverFault(i), s.nowMs, cfg::DRIVER_FAULT_HOLD_MS))
        r.add(FAULT_DRIVER, wheelComponent(i), Symptom::StuckHigh);
    }
    if (s.steerSensed && driverFault_[PowerMonitor::STEER].update(s.power->driverFault(PowerMonitor::STEER), s.nowMs,
                                                                  cfg::DRIVER_FAULT_HOLD_MS))
      r.add(FAULT_DRIVER, Component::Steer, Symptom::StuckHigh);
    float total = s.power->driveAmps();
    if (totalOvercurrent_.update(total > cfg::TOTAL_CURRENT_LIMIT_A, s.nowMs, cfg::OVERCURRENT_HOLD_MS))
      r.add(FAULT_OVERCURRENT, Component::Wheels, Symptom::OverLimit, total, cfg::TOTAL_CURRENT_LIMIT_A);
  }

  if (s.contactor) {
    if (s.contactor->busMissing())
      r.add(FAULT_NO_BUS, Component::K1,
            s.contactor->state() == ContactorState::WaitingForDischarge ? Symptom::Waiting : Symptom::Missing);
    if (s.contactor->welded()) r.add(FAULT_K1_WELDED, Component::K1, Symptom::Welded);
  }
  if (s.boxHot)
    r.add(FAULT_OVERTEMP, Component::Box, s.boxSensorLost ? Symptom::Stale : Symptom::OverLimit, s.boxCelsius,
          cfg::BOX_TEMP_LIMIT_C);
  if (s.tilted) r.add(FAULT_TILT, Component::Imu, Symptom::OverLimit);
  return r.mask();
}

bool SafetyManager::apply(Mode mode, const FaultLatch& faults, bool driveStopped, bool contactorAllowed,
                          KillLines& lines) {
  bool cut = mode == Mode::Killed || faults.has(HARD_FAULTS);
  bool driving = mode == Mode::Kid || mode == Mode::Parent;
  lines.setDrive(!cut && (driving || !driveStopped));
  // After the enable, so on a driver or over-current fault the drivers are
  // already off when K1 opens.
  lines.setContactor(contactorAllowed && !faults.has(CONTACTOR_FAULTS));
  return cut;
}
