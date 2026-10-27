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
#include "ChannelMap.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"
#include "Ramp.h"
#include "SafetyRules.h"
#include "config.h"
#include "control_config.h"
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
const SafetyLimits LIMITS = cfg::carControl().safety;  // the car's limits

KillLines lines;
RadioInput radio;
PowerMonitor power;
BTS7960Motor motor("FL", pins::FL_RPWM, pins::FL_LPWM, cfg::WHEEL_REVERSED[0]);
FaultLatch faults;
ModeMachine modes;
Ramp ramp;
SafetyRules rules;
uint32_t nextTickMs = 0;
uint32_t lastLogMs = 0;
}  // namespace

void setup() {
  lines.begin();
  logx::begin(115200);
  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, cfg::I2C_HZ);
  Wire.setTimeOut(cfg::I2C_TIMEOUT_MS);
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

  // No pedal, E-stop sense, bus sense, box sensor or IMU on the bench: one wheel's current only.
  CurrentSense current = power.snapshot(now);
  SafetyInputs in = {};
  in.link = link;
  in.killSwitch = kill;
  in.radioChannelsValid = true;
  in.manualOk = true;
  in.batteryVolts = power.batteryVolts();
  in.current = &current;
  in.wheelsSensed = 1;
  in.nowMs = now;
  faults.update(rules.evaluate(in, LIMITS));

  bool arming = armingOk({thrUs, cfg::CH_CENTER, 0.0f}, ARMING);
  Mode mode = modes.update({kill, req, arming}, faults);

  KillLineState kl = killLines(mode, faults, ramp.stopped(RAMP), true);  // no bus sense: no weld check
  if (kl.cut) {
    ramp.reset();
  } else {
    float cmd = mode == Mode::Parent ? limitCommand(stickFraction(thrUs, CHANNELS), 1.0f, cfg::REVERSE_CAP) : 0.0f;
    ramp.step(cmd, dtS, RAMP);
  }
  // The enable before K1: on a driver or over-current fault the drivers are off before K1 opens.
  lines.setDrive(kl.drive);
  lines.setContactor(kl.contactor);
  motor.write(ramp.value());

  if (logx::every(lastLogMs, 200)) {
    logx::line("%s link=%s out=%+.2f I=%.2fA bat=%.1fV en=%d fault=%s", modeName(mode), linkName(link),
               motor.output(), power.amps(PowerMonitor::FL), power.batteryVolts(), lines.drive(),
               faultName(faults.latched()));
  }
}
