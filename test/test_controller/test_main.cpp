// Whole-tick scenarios: the car's decision path (rideon::Controller) driven
// tick by tick at LOOP_HZ with the real config.h values, as src/main.cpp
// runs it. Each test names the requirement it verifies (docs/VERIFICATION.md).
#include <math.h>
#include <stdio.h>
#include <unity.h>

#include "Controller.h"
#include "config.h"
#include "control_config.h"

using namespace rideon;

namespace {

const uint32_t TICK_MS = 1000 / cfg::LOOP_HZ;
const uint16_t LOW_US = cfg::CH_LOW, MID_US = cfg::CH_CENTER, HIGH_US = cfg::CH_HIGH;
const ControlConfig CFG = cfg::carControl();

// The car around the Controller: a radio that sends or stops, sticks,
// switches, pedal and sensors, all healthy unless a test says otherwise.
struct Car {
  Controller ctl{CFG};
  ControlInputs in = {};
  CurrentSense current = {};
  uint32_t now = 1000;
  bool radioOn = true;
  uint32_t lastPacketMs = 0;
  uint32_t radioOffMs = 0;
  ControlOutputs out = {};

  Car() {
    in.throttleUs = in.steeringUs = MID_US;
    in.killUs = HIGH_US;  // run
    in.modeUs = LOW_US;   // Locked
    in.speedCapUs = LOW_US;
    in.hornUs = LOW_US;
    in.manualOk = true;
    in.direction = 1;
    in.batteryVolts = 25.0f;
    for (bool& f : current.fresh) f = true;
    in.current = &current;
    in.contactorChecked = true;
    in.contactorAllowed = true;
    in.boxCelsius = 30.0f;
    in.dtS = TICK_MS / 1000.0f;
  }

  ControlOutputs tick() {
    if (radioOn) lastPacketMs = now;
    // AlfredoCRSF keeps isLinkUp() true for 300 ms after the last packet;
    // the channel values stay frozen at what they were.
    bool linkUp = radioOn || now - radioOffMs < 300;
    in.link = {linkUp, true, lastPacketMs, 100, now};
    in.nowMs = now;
    out = ctl.step(in);
    now += TICK_MS;
    return out;
  }
  void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += TICK_MS) tick();
  }
  void radioOff() {
    radioOn = false;
    radioOffMs = now;
  }

  // Mode switch through Locked to `modeUs`, sticks centred, pedal released.
  void arm(uint16_t modeUs) {
    uint16_t thr = in.throttleUs;
    float pedal = in.pedal;
    in.throttleUs = MID_US;
    in.pedal = 0;
    in.modeUs = LOW_US;
    run(50);
    in.modeUs = modeUs;
    run(50);
    in.throttleUs = thr;
    in.pedal = pedal;
  }
  void driveParentAtFullSpeed() {
    arm(HIGH_US);
    TEST_ASSERT_EQUAL(Mode::Parent, out.mode);
    in.throttleUs = HIGH_US;
    run(2500);  // 0 to full in 1 / ACCEL_PER_S = 2 s
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, out.speed);
  }
};

Car* car;

}  // namespace

void setUp() { car = new Car(); }
void tearDown() { delete car; }

// ---- Configuration -------------------------------------------------------------

void test_config_is_the_car_config() {
  // Guards against a test config drifting from config.h.
  TEST_ASSERT_EQUAL_UINT32(cfg::LINK_TIMEOUT_MS, CFG.link.timeoutMs);
  TEST_ASSERT_EQUAL_FLOAT(cfg::BRAKE_PER_S, CFG.parentRamp.brakePerS);
  TEST_ASSERT_EQUAL_FLOAT(cfg::BRAKE_PER_S, CFG.kidRamp.brakePerS);
  TEST_ASSERT_EQUAL_FLOAT(cfg::MOTOR_CURRENT_LIMIT_A, CFG.safety.motorCurrentA);
  TEST_ASSERT_EQUAL_INT(4, CFG.wheelsSensed);
}

// ---- Boot ---------------------------------------------------------------------------

// SR-01: the car boots Locked and does not drive until the mode switch has
// been seen at Locked, even with the switch already at Parent and the stick held.
void test_boots_locked_and_needs_switch_cycle() {
  car->in.modeUs = HIGH_US;
  car->in.throttleUs = HIGH_US;
  car->run(1000);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_FALSE(car->out.lines.drive);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
  car->in.throttleUs = MID_US;
  car->run(100);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);  // still no switch cycle
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
}

// SR-13: no link before the first packet: Locked, nothing moves.
void test_no_link_at_boot_keeps_locked() {
  car->radioOn = false;
  car->radioOffMs = 0;
  car->lastPacketMs = 0;
  car->in.modeUs = HIGH_US;
  car->run(1000);
  TEST_ASSERT_EQUAL(LinkState::Lost, car->out.link);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_FALSE(car->out.lines.drive);
}

// SR-12: the contactor stays open while the boot weld check runs.
void test_contactor_open_until_weld_check_passes() {
  car->in.contactorAllowed = false;
  car->in.busMissing = true;
  car->in.busWaiting = true;
  car->run(500);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_NO_BUS));
  // Check passes, bus comes up: a re-arm clears NO_BUS and the car drives.
  car->in.contactorAllowed = true;
  car->in.busMissing = car->in.busWaiting = false;
  car->tick();
  TEST_ASSERT_TRUE(car->out.lines.contactor);
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
  TEST_ASSERT_FALSE(car->ctl.faults().any());
}

// SR-12: a welded contactor is permanent: no re-arm clears it.
void test_welded_contactor_never_clears() {
  car->in.k1Welded = true;
  car->tick();
  car->in.k1Welded = false;
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_K1_WELDED));
  TEST_ASSERT_FALSE(car->out.lines.contactor);
}

// SR-14: a part that failed at setup locks the car until a reboot.
void test_setup_failure_is_permanent() {
  car->ctl.raise(FAULT_SETUP);
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_TRUE(car->out.lines.cut);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
}

// ---- Link loss -------------------------------------------------------------------------

// SR-02: radio off at full speed: braked to zero within LINK_TIMEOUT_MS plus
// the braking time (1 / BRAKE_PER_S), drive enable held while braking and
// dropped once stopped. This is the firmware's share of the SAFETY.md stop time.
void test_link_loss_at_full_speed_brakes_to_zero_within_budget() {
  car->driveParentAtFullSpeed();
  car->radioOff();
  uint32_t off = car->now;
  const uint32_t budget = cfg::LINK_TIMEOUT_MS + (uint32_t)(1000.0f / cfg::BRAKE_PER_S) + 2 * TICK_MS;
  uint32_t stoppedAt = 0;
  float last = car->out.speed;
  while (car->now - off < budget + 500) {
    car->tick();
    TEST_ASSERT_TRUE_MESSAGE(car->out.speed <= last + 1e-6f, "speed never rises after link loss");
    last = car->out.speed;
    if (car->out.speed != 0.0f) TEST_ASSERT_TRUE_MESSAGE(car->out.lines.drive, "enable held while braking");
    if (car->out.speed == 0.0f && stoppedAt == 0) stoppedAt = car->now - TICK_MS;
  }
  TEST_ASSERT_TRUE(stoppedAt != 0);
  char msg[64];
  snprintf(msg, sizeof msg, "stopped %lu ms after the radio went off", (unsigned long)(stoppedAt - off));
  TEST_ASSERT_TRUE_MESSAGE(stoppedAt - off <= budget, msg);
  TEST_ASSERT_FALSE(car->out.lines.drive);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_LINK_LOST));
}

// SR-02: braking starts only after the timeout, never by channel values: a
// frozen full throttle is not obeyed once the link is lost.
void test_link_loss_ignores_frozen_channels() {
  car->driveParentAtFullSpeed();
  car->radioOff();
  car->run(cfg::LINK_TIMEOUT_MS + 2 * TICK_MS);
  TEST_ASSERT_EQUAL(LinkState::Lost, car->out.link);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.command);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.steering);
  car->in.hornUs = HIGH_US;  // frozen horn switch
  car->tick();
  TEST_ASSERT_FALSE(car->out.horn);
}

// SR-03: the link coming back never restarts the car, even with the throttle held.
void test_link_back_does_not_restart_without_rearm() {
  car->driveParentAtFullSpeed();
  car->radioOff();
  car->run(1500);
  car->radioOn = true;
  car->run(2000);  // stick still at full, switch still at Parent
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
  car->in.modeUs = LOW_US;  // switch to Locked with the stick held: no re-arm
  car->run(200);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_LINK_LOST));
  car->in.throttleUs = MID_US;
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
}

// SR-04: a weak link halves the speed cap.
void test_degraded_link_caps_speed() {
  car->arm(HIGH_US);
  car->in.throttleUs = HIGH_US;
  car->radioOn = true;
  for (int i = 0; i < 300; i++) {
    car->lastPacketMs = car->now;
    car->in.link = {true, true, car->now, (uint8_t)(cfg::LQ_DEGRADED_BELOW - 1), car->now};
    car->in.nowMs = car->now;
    car->out = car->ctl.step(car->in);
    car->now += TICK_MS;
  }
  TEST_ASSERT_EQUAL(LinkState::Degraded, car->out.link);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg::PARENT_CAP * cfg::LQ_DEGRADED_CAP, car->out.command);
  TEST_ASSERT_TRUE(car->out.speed <= cfg::PARENT_CAP * cfg::LQ_DEGRADED_CAP + 1e-6f);
}

// ---- Kill switch and E-stop -------------------------------------------------------------

// SR-05: kill switch: drive cut the same tick (no ramp), contactor opened, Killed.
void test_kill_switch_cuts_same_tick() {
  car->driveParentAtFullSpeed();
  car->in.killUs = LOW_US;
  car->tick();
  TEST_ASSERT_EQUAL(Mode::Killed, car->out.mode);
  TEST_ASSERT_TRUE(car->out.lines.cut);
  TEST_ASSERT_FALSE(car->out.lines.drive);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
}

// SR-05: releasing the kill switch goes to Locked, never straight back to driving.
void test_kill_release_needs_rearm() {
  car->driveParentAtFullSpeed();
  car->in.killUs = LOW_US;
  car->run(100);
  car->in.killUs = HIGH_US;
  car->run(500);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_FALSE(car->out.lines.drive);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
  car->in.throttleUs = MID_US;
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
  TEST_ASSERT_TRUE(car->out.lines.contactor);
}

// SR-05: a kill channel out of range counts as kill.
void test_invalid_kill_channel_is_kill() {
  car->driveParentAtFullSpeed();
  car->in.killUs = 2500;
  car->tick();
  TEST_ASSERT_EQUAL(Mode::Killed, car->out.mode);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
}

// SR-06: E-stop: cut at once and K1 held open after it is released, until a re-arm.
void test_estop_release_does_not_restart() {
  car->driveParentAtFullSpeed();
  car->in.estopPressed = true;
  car->tick();
  TEST_ASSERT_TRUE(car->out.lines.cut);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
  car->in.estopPressed = false;
  car->run(1000);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_FALSE(car->out.lines.contactor);
  TEST_ASSERT_FALSE(car->out.lines.drive);
  car->in.throttleUs = MID_US;
  car->arm(HIGH_US);
  TEST_ASSERT_TRUE(car->out.lines.contactor);
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
}

// ---- Every fault: hard or soft, contactor or not ------------------------------------------

namespace {

struct FaultCase {
  FaultMask fault;
  const char* name;
  void (*set)(Car&, bool on);
  uint32_t holdMs;  // how long the condition must last before it trips
};

void setLink(Car& c, bool on) {
  if (on) c.radioOff();
  else c.radioOn = true;
}
void setKill(Car& c, bool on) { c.in.killUs = on ? LOW_US : HIGH_US; }
void setBatteryLow(Car& c, bool on) { c.in.batteryVolts = on ? cfg::BATTERY_CUTOFF_V - 1.0f : 25.0f; }
void setBatterySensor(Car& c, bool on) { c.in.batteryVolts = on ? cfg::BATTERY_SENSOR_MIN_V - 1.0f : 25.0f; }
void setOvercurrent(Car& c, bool on) { c.current.amps[2] = on ? cfg::MOTOR_CURRENT_LIMIT_A + 1.0f : 5.0f; }
void setDriver(Car& c, bool on) { c.current.driverFault[1] = on; }
void setInput(Car& c, bool on) { c.in.steeringUs = on ? cfg::CH_MAX_VALID + 100 : MID_US; }
void setTilt(Car& c, bool on) { c.in.tilted = on; }
void setEstop(Car& c, bool on) { c.in.estopPressed = on; }
void setCurrentSense(Car& c, bool on) { c.current.fresh[3] = !on; }
void setNoBus(Car& c, bool on) { c.in.busMissing = on; }
void setOvertemp(Car& c, bool on) { c.in.boxHot = on; }

const FaultCase CASES[] = {
    {FAULT_LINK_LOST, "LINK_LOST", setLink, cfg::LINK_TIMEOUT_MS},
    {FAULT_KILL_SWITCH, "KILL", setKill, 0},
    {FAULT_BATTERY_LOW, "BATTERY_LOW", setBatteryLow, cfg::BATTERY_CUTOFF_HOLD_MS},
    {FAULT_BATTERY_SENSOR, "BATTERY_SENSOR", setBatterySensor, 0},
    {FAULT_OVERCURRENT, "OVERCURRENT", setOvercurrent, cfg::OVERCURRENT_HOLD_MS},
    {FAULT_DRIVER, "DRIVER", setDriver, cfg::DRIVER_FAULT_HOLD_MS},
    {FAULT_INPUT, "INPUT", setInput, 0},
    {FAULT_TILT, "TILT", setTilt, 0},
    {FAULT_ESTOP, "ESTOP", setEstop, 0},
    {FAULT_CURRENT_SENSE, "CURRENT_SENSE", setCurrentSense, 0},
    {FAULT_NO_BUS, "NO_BUS", setNoBus, 0},
    {FAULT_OVERTEMP, "OVERTEMP", setOvertemp, 0},
};

}  // namespace

// SR-07, SR-08, SR-09: every fault, injected at full speed. A HARD_FAULT cuts
// the drive the tick it latches; any other fault brakes to a stop at
// BRAKE_PER_S with the enable held. CONTACTOR_FAULTS open K1. Every fault
// stays latched after its condition ends, and a re-arm clears it.
void test_every_fault_stops_the_car_its_way() {
  FaultMask covered = FAULT_SETUP | FAULT_K1_WELDED;  // tested above
  for (const FaultCase& fc : CASES) {
    delete car;
    car = new Car();
    char msg[80];
    car->driveParentAtFullSpeed();
    fc.set(*car, true);
    uint32_t start = car->now;
    while (!car->ctl.faults().has(fc.fault)) {
      car->tick();
      snprintf(msg, sizeof msg, "%s did not trip within its hold time", fc.name);
      TEST_ASSERT_TRUE_MESSAGE(car->now - start <= fc.holdMs + 3 * TICK_MS, msg);
    }
    bool hard = (fc.fault & HARD_FAULTS) != 0;
    bool opensK1 = (fc.fault & CONTACTOR_FAULTS) != 0;
    snprintf(msg, sizeof msg, "%s: cut the tick it latches = %d", fc.name, hard);
    TEST_ASSERT_EQUAL_MESSAGE(hard, car->out.lines.cut, msg);
    snprintf(msg, sizeof msg, "%s: contactor open = %d", fc.name, opensK1);
    TEST_ASSERT_EQUAL_MESSAGE(!opensK1, car->out.lines.contactor, msg);
    if (hard) {
      snprintf(msg, sizeof msg, "%s: output zero at once", fc.name);
      TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, car->out.speed, msg);
      TEST_ASSERT_FALSE(car->out.lines.drive);
    } else {
      snprintf(msg, sizeof msg, "%s: brakes, enable held", fc.name);
      TEST_ASSERT_TRUE_MESSAGE(car->out.speed > 0.9f && car->out.lines.drive, msg);
      car->run((uint32_t)(1000.0f / cfg::BRAKE_PER_S) + 2 * TICK_MS);
      snprintf(msg, sizeof msg, "%s: stopped within the braking time", fc.name);
      TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, car->out.speed, msg);
      TEST_ASSERT_FALSE(car->out.lines.drive);
    }
    TEST_ASSERT_NOT_EQUAL(Mode::Parent, car->out.mode);

    // Condition ends: still latched, still stopped.
    fc.set(*car, false);
    car->run(500);
    snprintf(msg, sizeof msg, "%s stays latched", fc.name);
    TEST_ASSERT_TRUE_MESSAGE(car->ctl.faults().has(fc.fault), msg);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
    // Re-arm with the stick centred clears it.
    car->in.throttleUs = MID_US;
    car->arm(HIGH_US);
    snprintf(msg, sizeof msg, "%s cleared by a re-arm", fc.name);
    TEST_ASSERT_FALSE_MESSAGE(car->ctl.faults().any(), msg);
    TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
    covered |= fc.fault;
  }
  // Every fault code in Faults.h is in this table or tested above.
  const FaultMask ALL = FAULT_LINK_LOST | FAULT_KILL_SWITCH | FAULT_BATTERY_LOW | FAULT_BATTERY_SENSOR |
                        FAULT_OVERCURRENT | FAULT_DRIVER | FAULT_INPUT | FAULT_TILT | FAULT_SETUP | FAULT_ESTOP |
                        FAULT_CURRENT_SENSE | FAULT_NO_BUS | FAULT_K1_WELDED | FAULT_OVERTEMP;
  TEST_ASSERT_EQUAL_HEX16(ALL, covered);
}

// SR-10: a fault whose condition is still present is not cleared by a re-arm.
void test_rearm_does_not_clear_a_present_fault() {
  car->in.tilted = true;
  car->arm(HIGH_US);
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_TILT));
}

// SR-09: start-up inrush shorter than OVERCURRENT_HOLD_MS is not a fault;
// one tick longer is.
void test_overcurrent_hold_time() {
  car->driveParentAtFullSpeed();
  car->current.amps[0] = cfg::MOTOR_CURRENT_LIMIT_A + 5.0f;
  car->run(cfg::OVERCURRENT_HOLD_MS - TICK_MS);
  TEST_ASSERT_FALSE(car->ctl.faults().any());
  car->current.amps[0] = 5.0f;
  car->run(100);
  TEST_ASSERT_FALSE(car->ctl.faults().any());
  car->current.amps[0] = cfg::MOTOR_CURRENT_LIMIT_A + 5.0f;
  car->run(cfg::OVERCURRENT_HOLD_MS + TICK_MS);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_OVERCURRENT));
  TEST_ASSERT_TRUE(car->out.lines.cut);
}

// SR-09: four wheels each under their limit but together over the total limit.
void test_total_current_limit() {
  car->driveParentAtFullSpeed();
  for (int i = 0; i < 4; i++) car->current.amps[i] = cfg::TOTAL_CURRENT_LIMIT_A / 4 + 1.0f;
  car->current.driveAmps = cfg::TOTAL_CURRENT_LIMIT_A + 4.0f;
  car->run(cfg::OVERCURRENT_HOLD_MS + TICK_MS);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_OVERCURRENT));
  TEST_ASSERT_TRUE(car->out.lines.cut);
}

// SR-09: battery sag shorter than the hold time is not a cutoff.
void test_battery_sag_is_not_cutoff() {
  car->driveParentAtFullSpeed();
  car->in.batteryVolts = cfg::BATTERY_CUTOFF_V - 2.0f;
  car->run(cfg::BATTERY_CUTOFF_HOLD_MS - 100);
  car->in.batteryVolts = 25.0f;
  car->run(100);
  TEST_ASSERT_FALSE(car->ctl.faults().any());
  TEST_ASSERT_EQUAL(Mode::Parent, car->out.mode);
}

// ---- Re-arming ---------------------------------------------------------------------------------

// SR-10: a re-arm needs both sticks centred and the pedal released.
void test_rearm_needs_sticks_centred_and_pedal_released() {
  car->in.tilted = true;
  car->tick();
  car->in.tilted = false;
  car->in.modeUs = LOW_US;

  car->in.throttleUs = MID_US + cfg::ARM_CENTER_TOLERANCE_US + 20;
  car->run(200);
  TEST_ASSERT_TRUE(car->ctl.faults().any());
  car->in.throttleUs = MID_US;
  car->in.steeringUs = MID_US - cfg::ARM_CENTER_TOLERANCE_US - 20;
  car->run(200);
  TEST_ASSERT_TRUE(car->ctl.faults().any());
  car->in.steeringUs = MID_US;
  car->in.pedal = 0.5f;
  car->run(200);
  TEST_ASSERT_TRUE(car->ctl.faults().any());
  car->in.pedal = 0.0f;
  car->run(50);
  TEST_ASSERT_FALSE(car->ctl.faults().any());
}

// SR-11: Kid <-> Parent while the stick or pedal is held drops to Locked.
void test_mode_change_while_held_goes_locked() {
  car->arm(MID_US);  // Kid
  TEST_ASSERT_EQUAL(Mode::Kid, car->out.mode);
  car->in.pedal = 1.0f;
  car->run(500);
  car->in.modeUs = HIGH_US;  // to Parent with the pedal down
  car->tick();
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  car->run(1000);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
}

// SR-11: an invalid mode switch value locks the car.
void test_invalid_mode_switch_locks() {
  car->driveParentAtFullSpeed();
  car->in.modeUs = 500;
  car->tick();
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_INPUT) || car->out.command == 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.command);
}

// ---- Kid mode -------------------------------------------------------------------------------------

// SR-15: the speed-cap switch sets the Kid cap; low or invalid is slowest.
void test_kid_speed_caps() {
  car->arm(MID_US);
  car->in.pedal = 1.0f;
  const struct {
    uint16_t us;
    float cap;
  } caps[] = {{LOW_US, cfg::KID_CAP_MIN}, {MID_US, cfg::KID_CAP_MID}, {HIGH_US, cfg::KID_CAP_MAX}, {3000, cfg::KID_CAP_MIN}};
  for (auto& c : caps) {
    car->in.speedCapUs = c.us;
    car->tick();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, c.cap, car->out.command);
  }
}

// SR-15: reverse is capped to REVERSE_CAP of the forward cap.
void test_kid_reverse_cap() {
  car->arm(MID_US);
  car->in.speedCapUs = HIGH_US;
  car->in.pedal = 1.0f;
  car->in.direction = -1;
  car->run(5000);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -cfg::KID_CAP_MAX * cfg::REVERSE_CAP, car->out.command);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -cfg::KID_CAP_MAX * cfg::REVERSE_CAP, car->out.speed);
}

// SR-16: in Kid mode the parent pulling the stick back brakes, pedal ignored.
void test_parent_brake_override_in_kid_mode() {
  car->arm(MID_US);
  car->in.speedCapUs = HIGH_US;
  car->in.pedal = 1.0f;
  car->run(2000);
  TEST_ASSERT_TRUE(car->out.speed > 0.5f);
  car->in.throttleUs = LOW_US;
  car->run((uint32_t)(1000.0f / cfg::BRAKE_PER_S) + 2 * TICK_MS);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.speed);
  TEST_ASSERT_EQUAL(Mode::Kid, car->out.mode);
}

// SR-16: Kid mode with the pedal expander lost is an INPUT fault.
void test_kid_mode_needs_the_pedal_expander() {
  car->arm(MID_US);
  car->in.pedal = 1.0f;
  car->run(1000);
  car->in.manualOk = false;
  car->tick();
  TEST_ASSERT_TRUE(car->ctl.faults().has(FAULT_INPUT));
  TEST_ASSERT_EQUAL(Mode::Locked, car->out.mode);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, car->out.command);
}

// SR-17: Kid mode accelerates gentler than Parent.
void test_kid_ramp_is_gentler() {
  car->arm(MID_US);
  car->in.speedCapUs = HIGH_US;
  car->in.pedal = 1.0f;
  car->run(1000);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, cfg::KID_ACCEL_PER_S * 1.0f, car->out.speed);
}

// SR-17: forward to reverse brakes to a stop first; never a direct reversal.
void test_direction_change_stops_first() {
  car->driveParentAtFullSpeed();
  car->in.throttleUs = LOW_US;
  float last = car->out.speed;
  for (int i = 0; i < 300; i++) {
    car->tick();
    TEST_ASSERT_FALSE_MESSAGE(last > cfg::STOPPED_EPSILON && car->out.speed < 0.0f, "reversed while moving forward");
    last = car->out.speed;
  }
  TEST_ASSERT_TRUE(car->out.speed < 0.0f);
}

// ---- Invariants over random input ---------------------------------------------------------------

namespace {
uint32_t rng = 12345;
uint32_t next() {
  rng = rng * 1664525u + 1013904223u;
  return rng >> 8;
}
bool chance(int percent) { return (int)(next() % 100) < percent; }
const uint16_t SWITCH_US[] = {LOW_US, MID_US, HIGH_US};
uint16_t randomUs() {
  static const uint16_t VALUES[] = {LOW_US, MID_US, HIGH_US, 1400, 1600, 800, 2200};
  return VALUES[next() % 7];
}
}  // namespace

// SR-18: 200 000 ticks of random sticks, switches, pedal, faults and link
// drops. Every tick the safety invariants hold:
//   - a latched HARD_FAULT or Killed: drive enable LOW and output zero
//   - driving (Kid or Parent) only with no latched fault
//   - the contactor closed only with no CONTACTOR_FAULT latched and the weld check passed
//   - the output moves by at most one ramp step per tick, or straight to zero on a cut
//   - the output never goes from forward to reverse in one tick
//   - the output stays within the mode's cap
//   - the drive enable is HIGH only while driving or still braking to a stop
void test_invariants_hold_under_random_input() {
  rng = 12345;
  const float maxStep = (cfg::BRAKE_PER_S > cfg::ACCEL_PER_S ? cfg::BRAKE_PER_S : cfg::ACCEL_PER_S) * TICK_MS / 1000.0f;
  float last = 0.0f;
  int drivingTicks = 0, cutTicks = 0;
  int rearm = 0;
  for (int i = 0; i < 200000; i++) {
    Car& c = *car;
    // The parent and child: sticks, switches and pedal. Now and then, while
    // the car is stopped, the parent re-arms: sticks centred, pedal released,
    // mode switch to Locked and on to Kid or Parent.
    if (rearm == 0 && (c.out.mode == Mode::Locked || c.out.mode == Mode::Killed) && chance(2)) rearm = 20;
    if (rearm > 0) {
      c.in.throttleUs = c.in.steeringUs = MID_US;
      c.in.pedal = 0;
      c.in.killUs = HIGH_US;
      c.in.modeUs = rearm > 10 ? LOW_US : (rearm == 10 ? SWITCH_US[1 + next() % 2] : c.in.modeUs);
      rearm--;
    } else {
      if (chance(5)) c.in.throttleUs = randomUs();
      if (chance(5)) c.in.steeringUs = randomUs();
      if (chance(1)) c.in.modeUs = chance(95) ? SWITCH_US[next() % 3] : randomUs();
      if (next() % 1000 < 2) c.in.killUs = chance(50) ? LOW_US : randomUs();
      if (chance(2)) c.in.speedCapUs = randomUs();
      if (chance(3)) c.in.pedal = (next() % 5) / 4.0f;
      if (chance(1)) c.in.direction = (int8_t)((int)(next() % 3) - 1);
    }
    // Now and then one thing goes wrong, and later everything heals.
    if (next() % 1000 < 3) {
      switch (next() % 12) {
        case 0: c.in.manualOk = false; break;
        case 1: c.in.estopPressed = true; break;
        case 2: c.in.tilted = true; break;
        case 3: c.in.batteryVolts = chance(50) ? cfg::BATTERY_CUTOFF_V - 1.0f : 3.0f; break;
        case 4: c.current.amps[next() % 4] = cfg::MOTOR_CURRENT_LIMIT_A + 10.0f; break;
        case 5: c.current.driverFault[next() % 5] = true; break;
        case 6: c.current.fresh[next() % 6] = false; break;
        case 7: c.in.boxHot = true; break;
        case 8: c.in.busMissing = true; break;
        case 9: c.in.contactorAllowed = false; break;
        case 10: c.radioOff(); break;
        default: c.in.killUs = 2500; break;  // channel out of range
      }
    }
    if (chance(1)) {
      c.in.manualOk = true;
      c.in.estopPressed = c.in.tilted = c.in.boxHot = c.in.busMissing = false;
      c.in.batteryVolts = 25.0f;
      for (int w = 0; w < 5; w++) {
        c.current.amps[w] = 5.0f;
        c.current.driverFault[w] = false;
      }
      for (bool& f : c.current.fresh) f = true;
      c.in.contactorAllowed = true;
      c.radioOn = true;
    }

    ControlOutputs o = c.tick();
    const FaultLatch& f = c.ctl.faults();
    bool driving = o.mode == Mode::Kid || o.mode == Mode::Parent;
    char msg[96];
    snprintf(msg, sizeof msg, "tick %d mode %s faults 0x%04X speed %.3f last %.3f", i, modeName(o.mode), f.latched(),
             o.speed, last);

    if (f.has(HARD_FAULTS) || o.mode == Mode::Killed) {
      TEST_ASSERT_FALSE_MESSAGE(o.lines.drive, msg);
      TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, o.speed, msg);
      TEST_ASSERT_TRUE_MESSAGE(o.lines.cut, msg);
      cutTicks++;
    }
    if (driving) {
      TEST_ASSERT_FALSE_MESSAGE(f.any(), msg);
      drivingTicks++;
    } else {
      TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, o.command, msg);
    }
    if (o.lines.contactor) {
      TEST_ASSERT_FALSE_MESSAGE(f.has(CONTACTOR_FAULTS), msg);
      TEST_ASSERT_TRUE_MESSAGE(c.in.contactorAllowed, msg);
    }
    // One ramp step, plus the snap to zero once under STOPPED_EPSILON.
    if (!o.lines.cut) TEST_ASSERT_TRUE_MESSAGE(fabsf(o.speed - last) <= maxStep + cfg::STOPPED_EPSILON, msg);
    TEST_ASSERT_FALSE_MESSAGE(last > cfg::STOPPED_EPSILON && o.speed < -cfg::STOPPED_EPSILON, msg);
    TEST_ASSERT_FALSE_MESSAGE(last < -cfg::STOPPED_EPSILON && o.speed > cfg::STOPPED_EPSILON, msg);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(o.speed) <= 1.0f, msg);
    if (o.mode == Mode::Kid) TEST_ASSERT_TRUE_MESSAGE(fabsf(o.command) <= cfg::KID_CAP_MAX + 1e-6f, msg);
    if (o.lines.drive) TEST_ASSERT_TRUE_MESSAGE(driving || last != 0.0f, msg);
    last = o.speed;
  }
  // The run must actually exercise both driving and cutting, or it proves little.
  char counts[64];
  snprintf(counts, sizeof counts, "driving %d ticks, cut %d ticks", drivingTicks, cutTicks);
  printf("%s\n", counts);
  TEST_ASSERT_TRUE_MESSAGE(drivingTicks > 50000 && cutTicks > 10000, counts);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config_is_the_car_config);
  RUN_TEST(test_boots_locked_and_needs_switch_cycle);
  RUN_TEST(test_no_link_at_boot_keeps_locked);
  RUN_TEST(test_contactor_open_until_weld_check_passes);
  RUN_TEST(test_welded_contactor_never_clears);
  RUN_TEST(test_setup_failure_is_permanent);
  RUN_TEST(test_link_loss_at_full_speed_brakes_to_zero_within_budget);
  RUN_TEST(test_link_loss_ignores_frozen_channels);
  RUN_TEST(test_link_back_does_not_restart_without_rearm);
  RUN_TEST(test_degraded_link_caps_speed);
  RUN_TEST(test_kill_switch_cuts_same_tick);
  RUN_TEST(test_kill_release_needs_rearm);
  RUN_TEST(test_invalid_kill_channel_is_kill);
  RUN_TEST(test_estop_release_does_not_restart);
  RUN_TEST(test_every_fault_stops_the_car_its_way);
  RUN_TEST(test_rearm_does_not_clear_a_present_fault);
  RUN_TEST(test_overcurrent_hold_time);
  RUN_TEST(test_total_current_limit);
  RUN_TEST(test_battery_sag_is_not_cutoff);
  RUN_TEST(test_rearm_needs_sticks_centred_and_pedal_released);
  RUN_TEST(test_mode_change_while_held_goes_locked);
  RUN_TEST(test_invalid_mode_switch_locks);
  RUN_TEST(test_kid_speed_caps);
  RUN_TEST(test_kid_reverse_cap);
  RUN_TEST(test_parent_brake_override_in_kid_mode);
  RUN_TEST(test_kid_mode_needs_the_pedal_expander);
  RUN_TEST(test_kid_ramp_is_gentler);
  RUN_TEST(test_direction_change_stops_first);
  RUN_TEST(test_invariants_hold_under_random_input);
  return UNITY_END();
}
