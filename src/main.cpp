// RideOnESP32 v3 car firmware.
//
// setup() holds every kill line LOW until all outputs are zeroed, then the
// loop runs at a fixed 100 Hz: read inputs, judge the link, evaluate faults,
// decide the mode, compute the command, ramp, and set the kill lines.
// See docs/firmware/ARCHITECTURE.md.
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include "ArmingCheck.h"
#include "ChannelMap.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"
#include "Ramp.h"
#include "accessories/Accessories.h"
#include "config.h"
#include "drive/DriveController.h"
#include "drive/SteeringController.h"
#include "input/ManualInput.h"
#include "input/RadioInput.h"
#include "pins.h"
#include "safety/KillLines.h"
#include "safety/SafetyManager.h"
#include "safety/Watchdog.h"
#include "sensing/Imu.h"
#include "sensing/PowerMonitor.h"
#include "telemetry/Log.h"

using namespace rideon;

namespace {

const ChannelLimits CHANNELS = {cfg::CH_MIN_VALID, cfg::CH_MAX_VALID,      cfg::CH_LOW,          cfg::CH_CENTER,
                                cfg::CH_HIGH,      cfg::STICK_DEADBAND_US, cfg::SWITCH_LOW_BELOW, cfg::SWITCH_HIGH_ABOVE};
const LinkParams LINK = {cfg::LINK_TIMEOUT_MS, cfg::LQ_DEGRADED_BELOW, cfg::LQ_LOST_BELOW};
const ArmingParams ARMING = {cfg::CH_CENTER, cfg::ARM_CENTER_TOLERANCE_US, cfg::PEDAL_RELEASED_FRACTION};
const RampParams PARENT_RAMP = {cfg::ACCEL_PER_S, cfg::BRAKE_PER_S, cfg::STOPPED_EPSILON};
const RampParams KID_RAMP = {cfg::KID_ACCEL_PER_S, cfg::BRAKE_PER_S, cfg::STOPPED_EPSILON};

KillLines lines;
RadioInput radio;
ManualInput manual;
PowerMonitor power;
DriveController drive;
SteeringController steering;
SafetyManager safety;
Imu imu;
Accessories accessories;
FaultLatch faults;
ModeMachine modes;

uint32_t nextTickUs = 0;
uint32_t lastLogMs = 0;

const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "other";
  }
}

float kidSpeedCap(uint16_t us) {
  switch (switchPosition(us, CHANNELS)) {
    case 1: return cfg::KID_CAP_MID;
    case 2: return cfg::KID_CAP_MAX;
    default: return cfg::KID_CAP_MIN;  // low or invalid: slowest
  }
}

}  // namespace

void setup() {
  lines.begin();  // first: every kill line LOW
  logx::begin(115200);
  logx::line("RideOnESP32 v3 boot, reset reason: %s", resetReasonName(esp_reset_reason()));

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, 400000);

  bool ok = drive.begin();
  ok &= steering.begin();
  ok &= radio.begin();
  ok &= power.begin();
  if (!manual.begin()) logx::line("PCF8574 not answering: Kid mode refused");
  imu.begin();
  accessories.begin();

  if (!ok) {
    faults.raise(FAULT_SETUP);
    logx::line("SETUP FAILED: car stays locked until reboot");
  }

  watchdog::begin();
  nextTickUs = micros();
}

void loop() {
  radio.update();  // drain the receiver between ticks too
  uint32_t nowUs = micros();
  if ((int32_t)(nowUs - nextTickUs) < 0) return;
  nextTickUs += cfg::LOOP_PERIOD_US;
  if ((int32_t)(nowUs - nextTickUs) > (int32_t)cfg::LOOP_PERIOD_US) nextTickUs = nowUs;  // fell behind: resync
  const float dtS = cfg::LOOP_PERIOD_US / 1e6f;
  watchdog::feed();

  uint32_t now = millis();
  manual.update();
  power.update();
  imu.update();

  uint16_t throttleUs = radio.channel(ch::THROTTLE);
  uint16_t steeringUs = radio.channel(ch::STEERING);
  uint16_t killUs = radio.channel(ch::KILL);
  LinkState link = evaluateLink(radio.linkInputs(now), LINK);
  bool linkUsable = link != LinkState::Lost;

  // Channel values are trusted only while the link is up; they freeze when it drops.
  int killPos = switchPosition(killUs, CHANNELS);
  bool killSwitch = linkUsable && killPos != 2;  // only a clear "run" position allows driving
  ModeRequest request =
      linkUsable ? modeRequestFromSwitch(switchPosition(radio.channel(ch::MODE), CHANNELS)) : ModeRequest::Locked;
  bool channelsValid = !linkUsable || (channelValid(throttleUs, CHANNELS) && channelValid(steeringUs, CHANNELS) &&
                                       killPos >= 0);

  SafetySignals signals = {link,
                           killSwitch,
                           channelsValid,
                           manual.ok(),
                           manual.ok() && manual.estopPressed(),
                           request == ModeRequest::Kid,
                           power.batteryVolts(),
                           &power,
                           DriveController::WHEELS,
                           true,
                           imu.tilted(),
                           now};
  faults.update(safety.evaluate(signals));

  bool arming = armingOk({throttleUs, steeringUs, manual.pedal()}, ARMING);
  Mode mode = modes.update({killSwitch, request, arming}, faults);

  float linkCap = link == LinkState::Degraded ? cfg::LQ_DEGRADED_CAP : 1.0f;
  float throttle = stickFraction(throttleUs, CHANNELS);
  float command = 0.0f;
  const RampParams* ramp = &PARENT_RAMP;
  if (mode == Mode::Kid) {
    ramp = &KID_RAMP;
    float cap = kidSpeedCap(radio.channel(ch::SPEED_CAP)) * linkCap;
    bool parentBrake = throttle <= cfg::KID_BRAKE_OVERRIDE;
    command = parentBrake ? 0.0f : limitCommand(manual.pedal() * manual.direction(), cap, cfg::REVERSE_CAP);
  } else if (mode == Mode::Parent) {
    command = limitCommand(throttle, cfg::PARENT_CAP * linkCap, cfg::REVERSE_CAP);
  }

  bool cutNow = safety.apply(mode, faults, drive.stopped(*ramp), lines);
  if (cutNow) {
    drive.stopNow();
    steering.stopNow(lines);
  } else {
    drive.update(command, dtS, *ramp);
    steering.update(stickFraction(steeringUs, CHANNELS), mode == Mode::Parent, dtS, lines);
  }

  bool horn = (mode == Mode::Kid && manual.horn()) ||
              (linkUsable && switchPosition(radio.channel(ch::HORN), CHANNELS) == 2);
  accessories.update(horn, drive.speed(), safety.battery() != BatteryState::Ok);

  if (logx::every(lastLogMs, 500)) {
    logx::line("%s link=%s lq=%u bat=%.1fV I=%.1fA cmd=%.2f out=%.2f en=%d k=%d fault=%s", modeName(mode),
               linkName(link), radio.linkQuality(), power.batteryVolts(), power.driveAmps(), command, drive.speed(),
               lines.drive(), lines.contactor(), faultName(faults.latched()));
  }
}
