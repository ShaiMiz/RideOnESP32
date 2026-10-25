#include "SafetyManager.h"

#include "config.h"
#include "safety/KillLines.h"
#include "sensing/PowerMonitor.h"

using namespace rideon;

namespace {
const BatteryParams BATTERY = {cfg::BATTERY_SENSOR_MIN_V, cfg::BATTERY_SENSOR_MAX_V, cfg::BATTERY_LOW_WARN_V,
                               cfg::BATTERY_CUTOFF_V,     cfg::BATTERY_RECOVER_V,    cfg::BATTERY_HIGH_WARN_V,
                               cfg::BATTERY_CUTOFF_HOLD_MS};
}

FaultMask SafetyManager::evaluate(const SafetySignals& s) {
  FaultMask f = FAULT_NONE;

  if (s.link == LinkState::Lost) f |= FAULT_LINK_LOST;
  if (s.killSwitch) f |= FAULT_KILL_SWITCH;
  if (!s.radioChannelsValid) f |= FAULT_INPUT;
  if (s.kidRequested && !s.manualOk) f |= FAULT_INPUT;
  if (s.estop) f |= FAULT_ESTOP;

  switch (battery_.update(s.batteryVolts, s.nowMs, BATTERY)) {
    case BatteryState::SensorFault: f |= FAULT_BATTERY_SENSOR; break;
    case BatteryState::Cutoff: f |= FAULT_BATTERY_LOW; break;
    default: break;
  }

  if (s.power) {
    // Without fresh readings the over-current and driver checks below would
    // be judging old values, so a lost ADS1115 is a fault of its own.
    for (int i = 0; i < s.wheelsSensed; i++)
      if (!s.power->currentFresh(i, s.nowMs)) f |= FAULT_CURRENT_SENSE;
    if (s.steerSensed && !s.power->currentFresh(PowerMonitor::STEER, s.nowMs)) f |= FAULT_CURRENT_SENSE;
    if (s.busSensed && !s.power->currentFresh(PowerMonitor::BUS, s.nowMs)) f |= FAULT_CURRENT_SENSE;

    for (int i = 0; i < s.wheelsSensed; i++) {
      if (overcurrent_[i].update(s.power->amps(i) > cfg::MOTOR_CURRENT_LIMIT_A, s.nowMs, cfg::OVERCURRENT_HOLD_MS))
        f |= FAULT_OVERCURRENT;
      if (driverFault_[i].update(s.power->driverFault(i), s.nowMs, cfg::DRIVER_FAULT_HOLD_MS)) f |= FAULT_DRIVER;
    }
    if (s.steerSensed && driverFault_[PowerMonitor::STEER].update(s.power->driverFault(PowerMonitor::STEER), s.nowMs,
                                                                  cfg::DRIVER_FAULT_HOLD_MS))
      f |= FAULT_DRIVER;
    if (totalOvercurrent_.update(s.power->driveAmps() > cfg::TOTAL_CURRENT_LIMIT_A, s.nowMs,
                                 cfg::OVERCURRENT_HOLD_MS))
      f |= FAULT_OVERCURRENT;
  }

  if (s.contactor) {
    if (s.contactor->busMissing()) f |= FAULT_NO_BUS;
    if (s.contactor->welded()) f |= FAULT_K1_WELDED;
  }
  if (s.boxHot) f |= FAULT_OVERTEMP;
  if (s.tilted) f |= FAULT_TILT;
  return f;
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
