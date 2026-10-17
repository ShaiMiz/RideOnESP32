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
    for (int i = 0; i < s.wheelsSensed; i++) {
      if (overcurrent_[i].update(s.power->amps(i) > cfg::MOTOR_CURRENT_LIMIT_A, s.nowMs, cfg::OVERCURRENT_HOLD_MS))
        f |= FAULT_OVERCURRENT;
      if (s.power->driverFault(i)) f |= FAULT_DRIVER;
    }
    if (totalOvercurrent_.update(s.power->driveAmps() > cfg::TOTAL_CURRENT_LIMIT_A, s.nowMs,
                                 cfg::OVERCURRENT_HOLD_MS))
      f |= FAULT_OVERCURRENT;
  }

  if (s.tilted) f |= FAULT_TILT;
  return f;
}

bool SafetyManager::apply(Mode mode, const FaultLatch& faults, bool driveStopped, KillLines& lines) {
  lines.setContactor(!faults.has(CONTACTOR_FAULTS));

  if (mode == Mode::Killed || faults.has(HARD_FAULTS)) {
    lines.setDrive(false);
    return true;
  }
  bool driving = mode == Mode::Kid || mode == Mode::Parent;
  lines.setDrive(driving || !driveStopped);
  return false;
}
