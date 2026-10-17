// Build phase 4 bench firmware: one motor on one driver.
//
// The front-left IBT-2 drives one motor from the radio throttle (Parent
// position of the mode switch; Kid position is treated as Locked because
// there is no pedal on the bench). It uses the same safety core as the car:
// link monitor, fault latch, arming check, mode machine and ramps, plus the
// drive enable and contactor lines.
//
// Gate (docs/COMMISSIONING.md, phase 4):
//   - pulling the receiver plug stops the motor
//   - a held throttle never restarts the motor after a fault
//   - the logged current agrees with a clamp meter or multimeter
#include <Arduino.h>
#include <Wire.h>

#include "ArmingCheck.h"
#include "BatteryCheck.h"
#include "ChannelMap.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"
#include "Ramp.h"
#include "config.h"
#include "drive/BTS7960Motor.h"
#include "input/RadioInput.h"
#include "pins.h"
#include "safety/KillLines.h"
#include "safety/Watchdog.h"
#include "sensing/PowerMonitor.h"
#include "telemetry/Log.h"

using namespace rideon;

namespace {
const ChannelLimits CHANNELS = {cfg::CH_MIN_VALID, cfg::CH_MAX_VALID,      cfg::CH_LOW,          cfg::CH_CENTER,
                                cfg::CH_HIGH,      cfg::STICK_DEADBAND_US, cfg::SWITCH_LOW_BELOW, cfg::SWITCH_HIGH_ABOVE};
const LinkParams LINK = {cfg::LINK_TIMEOUT_MS, cfg::LQ_DEGRADED_BELOW, cfg::LQ_LOST_BELOW};
const ArmingParams ARMING = {cfg::CH_CENTER, cfg::ARM_CENTER_TOLERANCE_US, 1.0f};  // no pedal on the bench
const RampParams RAMP = {cfg::ACCEL_PER_S, cfg::BRAKE_PER_S, cfg::STOPPED_EPSILON};
const BatteryParams BATTERY = {cfg::BATTERY_SENSOR_MIN_V, cfg::BATTERY_SENSOR_MAX_V, cfg::BATTERY_LOW_WARN_V,
                               cfg::BATTERY_CUTOFF_V,     cfg::BATTERY_RECOVER_V,    cfg::BATTERY_HIGH_WARN_V,
                               cfg::BATTERY_CUTOFF_HOLD_MS};

KillLines lines;
RadioInput radio;
PowerMonitor power;
BTS7960Motor motor("FL", pins::FL_RPWM, pins::FL_LPWM);
FaultLatch faults;
ModeMachine modes;
Ramp ramp;
BatteryCheck battery;
HoldTimer overcurrent;
uint32_t nextTickMs = 0;
uint32_t lastLogMs = 0;
}  // namespace

void setup() {
  lines.begin();
  logx::begin(115200);
  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, 400000);
  bool ok = motor.begin(cfg::PWM_FREQ_HZ, cfg::PWM_BITS);
  ok &= radio.begin();
  ok &= power.begin(true);
  if (!ok) faults.raise(FAULT_SETUP);
  logx::line("one_motor: %s", ok ? "ready, put the mode switch to Locked" : "SETUP FAILED");
  watchdog::begin();
}

void loop() {
  radio.update();
  uint32_t now = millis();
  if ((int32_t)(now - nextTickMs) < 0) return;
  nextTickMs = now + 10;
  const float dtS = 0.01f;
  watchdog::feed();

  power.update();
  LinkState link = evaluateLink(radio.linkInputs(now), LINK);
  bool linkUsable = link != LinkState::Lost;
  uint16_t thrUs = radio.channel(ch::THROTTLE);
  bool kill = linkUsable && switchPosition(radio.channel(ch::KILL), CHANNELS) != 2;
  ModeRequest req =
      linkUsable ? modeRequestFromSwitch(switchPosition(radio.channel(ch::MODE), CHANNELS)) : ModeRequest::Locked;
  if (req == ModeRequest::Kid) req = ModeRequest::Locked;

  FaultMask active = FAULT_NONE;
  if (!linkUsable) active |= FAULT_LINK_LOST;
  if (kill) active |= FAULT_KILL_SWITCH;
  BatteryState bs = battery.update(power.batteryVolts(), now, BATTERY);
  if (bs == BatteryState::SensorFault) active |= FAULT_BATTERY_SENSOR;
  if (bs == BatteryState::Cutoff) active |= FAULT_BATTERY_LOW;
  if (overcurrent.update(power.amps(PowerMonitor::FL) > cfg::MOTOR_CURRENT_LIMIT_A, now, cfg::OVERCURRENT_HOLD_MS))
    active |= FAULT_OVERCURRENT;
  if (power.driverFault(PowerMonitor::FL)) active |= FAULT_DRIVER;
  faults.update(active);

  bool arming = armingOk({thrUs, cfg::CH_CENTER, 0.0f}, ARMING);
  Mode mode = modes.update({kill, req, arming}, faults);

  lines.setContactor(!faults.has(CONTACTOR_FAULTS));
  if (mode == Mode::Killed || faults.has(HARD_FAULTS)) {
    lines.setDrive(false);
    ramp.reset();
    motor.write(0.0f);
  } else {
    float cmd = mode == Mode::Parent ? limitCommand(stickFraction(thrUs, CHANNELS), 1.0f, cfg::REVERSE_CAP) : 0.0f;
    motor.write(ramp.step(cmd, dtS, RAMP));
    lines.setDrive(mode == Mode::Parent || !ramp.stopped(RAMP));
  }

  if (logx::every(lastLogMs, 200)) {
    logx::line("%s link=%s out=%+.2f I=%.2fA bat=%.1fV en=%d fault=%s", modeName(mode), linkName(link),
               motor.output(), power.amps(PowerMonitor::FL), power.batteryVolts(), lines.drive(),
               faultName(faults.latched()));
  }
}
