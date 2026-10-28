// RideOnESP32 v3 car firmware.
//
// setup() holds every kill line LOW until all outputs are zeroed, then the
// loop runs at a fixed 100 Hz: read the hardware, let rideon::Controller
// decide (link, faults, mode, command, ramp, kill lines), and write its
// outputs. Every decision is in the Controller so the PC tests cover it.
// See docs/firmware/ARCHITECTURE.md and docs/VERIFICATION.md.
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include "BoxTempCheck.h"
#include "ChannelMap.h"
#include "ContactorCheck.h"
#include "Controller.h"
#include "Faults.h"
#include "accessories/Accessories.h"
#include "config.h"
#include "control_config.h"
#include "drive/DriveController.h"
#include "drive/SteeringController.h"
#include "input/ManualInput.h"
#include "input/RadioInput.h"
#include "pins.h"
#include "safety/KillLines.h"
#include "safety/Watchdog.h"
#include "sensing/BoxTemp.h"
#include "sensing/Imu.h"
#include "sensing/PowerMonitor.h"
#include "telemetry/EventSink.h"
#include "telemetry/Log.h"
#include "telemetry/RadioTelemetry.h"
#include "telemetry/SerialConsole.h"

using namespace rideon;

namespace {

const ContactorParams CONTACTOR = {cfg::BUS_OPEN_MAX_FRACTION, cfg::BUS_CLOSED_MIN_FRACTION,
                                   cfg::BUS_DISCHARGE_TIMEOUT_MS, cfg::BUS_CLOSE_TIME_MS};
const BoxTempParams BOX_TEMP = {cfg::BOX_TEMP_LIMIT_C, cfg::BOX_TEMP_RECOVER_C, cfg::BOX_TEMP_HOLD_MS,
                                cfg::BOX_TEMP_STALE_MS, cfg::BOX_TEMP_REQUIRED};

KillLines lines;
RadioInput radio;
RadioTelemetry radioTelemetry;
ManualInput manual;
PowerMonitor power;
DriveController drive;
SteeringController steering;
Imu imu;
BoxTemp boxTemp;
Accessories accessories;
ContactorCheck contactor;
BoxTempCheck boxTempCheck;
Controller control(cfg::carControl());

uint32_t nextTickUs = 0;
uint32_t lastLogMs = 0;

ResetReason resetReason(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return ResetReason::PowerOn;
    case ESP_RST_SW: return ResetReason::Software;
    case ESP_RST_PANIC: return ResetReason::Panic;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return ResetReason::Watchdog;
    case ESP_RST_BROWNOUT: return ResetReason::Brownout;
    default: return ResetReason::Other;
  }
}

// A part that failed to start: logged and counted with its name.
bool started(bool ok, Component comp) {
  if (!ok) events::setupFailed(comp);
  return ok;
}

}  // namespace

void setup() {
  lines.begin();  // first: every kill line LOW
  logx::begin(115200);
  logx::line("RideOnESP32 v3 car firmware");
  events::begin(resetReason(esp_reset_reason()));  // the BOOT record carries the reset reason

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, cfg::I2C_HZ);
  Wire.setTimeOut(cfg::I2C_TIMEOUT_MS);

  bool ok = started(drive.begin(), Component::Wheels);
  ok &= started(steering.begin(), Component::Steer);
  ok &= started(radio.begin(), Component::Radio);
  ok &= started(power.begin(), Component::Ads);
  if (!manual.begin()) logx::line("PCF8574 not answering: Kid mode refused");
  imu.begin();
  boxTemp.begin();
  if (!accessories.begin()) logx::line("PCA9685 not answering: box fans OFF, retrying every second");

  if (!ok) {
    control.raise(FAULT_SETUP);
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
  boxTemp.update(now);

  float batteryV = power.batteryVolts();
  bool readingsValid = power.busReadingValid(now) && batteryV >= cfg::BATTERY_SENSOR_MIN_V &&
                       batteryV <= cfg::BATTERY_SENSOR_MAX_V;
  contactor.update(readingsValid, power.busVolts(), batteryV, lines.contactor(), now, CONTACTOR);
  boxTempCheck.update(boxTemp.hasReading(), boxTemp.celsius(), boxTemp.lastGoodMs(), now, BOX_TEMP);
  CurrentSense current = power.snapshot(now);

  ControlInputs in = {};
  in.link = radio.linkInputs(now);
  in.throttleUs = radio.channel(ch::THROTTLE);
  in.steeringUs = radio.channel(ch::STEERING);
  in.killUs = radio.channel(ch::KILL);
  in.modeUs = radio.channel(ch::MODE);
  in.speedCapUs = radio.channel(ch::SPEED_CAP);
  in.hornUs = radio.channel(ch::HORN);
  in.manualOk = manual.ok();
  in.estopPressed = manual.estopPressed();
  in.pedal = manual.pedal();
  in.direction = manual.direction();
  in.kidHorn = manual.horn();
  in.batteryVolts = batteryV;
  in.current = &current;
  in.contactorChecked = true;
  in.busMissing = contactor.busMissing();
  in.busWaiting = contactor.state() == ContactorState::WaitingForDischarge;
  in.k1Welded = contactor.welded();
  in.contactorAllowed = contactor.allowClose();
  in.boxHot = boxTempCheck.hot();
  in.boxSensorLost = boxTempCheck.sensorLost();
  in.boxCelsius = boxTemp.celsius();
  in.tilted = imu.tilted();
  in.nowMs = now;
  in.dtS = dtS;
  ControlOutputs out = control.step(in);

  // The enable first, so on a driver or over-current fault the drivers are
  // already off when K1 opens.
  lines.setDrive(out.lines.drive);
  lines.setContactor(out.lines.contactor);
  drive.write(out.speed);
  if (out.lines.cut) {
    steering.stopNow(lines);
  } else {
    steering.update(out.steering, out.mode == Mode::Parent, dtS, lines);
  }
  accessories.update(out.horn, out.speed, control.battery() != BatteryState::Ok);

  FreezeFrame frame = {};
  frame.linkQuality = radio.linkQuality();
  frame.command = scaled(out.command, 100.0f);
  frame.output = scaled(out.speed, 100.0f);
  frame.pedal = scaled(manual.pedal(), 100.0f);
  frame.batteryV = scaled(batteryV, 100.0f);
  frame.busV = scaled(power.busVolts(), 100.0f);
  for (int i = 0; i < PowerMonitor::CURRENTS; i++) frame.amps[i] = scaled(power.amps(i), 100.0f);
  int16_t boxC = boxTemp.hasReading() ? scaled(boxTemp.celsius(), 1.0f) : -128;
  frame.boxC = (int8_t)(boxC > 127 ? 127 : boxC < -128 ? -128 : boxC);
  events::update({now, out.mode, out.link, radio.linkQuality(), control.battery(), batteryV, power.driveAmps(),
                  out.speed, &control.faults(), &control.reports(), frame});
  // Flash erases and NVS writes stall the CPU: only with the drivers off and the car stopped.
  bool safeToStall = (out.mode == Mode::Locked || out.mode == Mode::Killed) && !lines.drive() && control.stopped();
  events::drain(cfg::LOG_RECORDS_PER_TICK, safeToStall);
  console::update({out.mode, out.link, radio.linkQuality(), control.faults().latched(), batteryV, power.busVolts(),
                   power.driveAmps(), out.speed, safeToStall, manual.horn() || out.horn,
                   (out.mode == Mode::Locked || out.mode == Mode::Killed) && control.stopped()});
  radioTelemetry.update(radio, {now, radio.linkUp(), out.mode, control.faults().latched(), batteryV,
                                power.driveAmps()});

  if (!console::busy() && logx::every(lastLogMs, 500)) {
    logx::line("%s link=%s lq=%u bat=%.1fV bus=%.1fV I=%.1fA cmd=%.2f out=%.2f en=%d k=%d box=%s%.0fC fan=%d fault=%s",
               modeName(out.mode), linkName(out.link), radio.linkQuality(), batteryV, power.busVolts(),
               power.driveAmps(), out.command, out.speed, lines.drive(), lines.contactor(),
               boxTempCheck.sensorLost() ? "?" : "", boxTemp.celsius(), accessories.fansOn(),
               faultName(control.faults().latched()));
  }
}
