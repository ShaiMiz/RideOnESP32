// The safety rules on their own: which input raises which fault on which
// part, and the kill-line truth table. The whole-tick behaviour is in
// test_controller.
#include <stdio.h>
#include <unity.h>

#include "SafetyRules.h"
#include "config.h"
#include "control_config.h"

using namespace rideon;

namespace {

const SafetyLimits LIM = cfg::carControl().safety;
SafetyRules* rules;
CurrentSense current;
SafetyInputs in;

void healthy() {
  current = {};
  for (bool& f : current.fresh) f = true;
  in = {};
  in.link = LinkState::Ok;
  in.radioChannelsValid = true;
  in.manualOk = true;
  in.batteryVolts = 25.0f;
  in.current = &current;
  in.wheelsSensed = 4;
  in.steerSensed = true;
  in.busSensed = true;
  in.contactorChecked = true;
  in.boxCelsius = 30.0f;
  in.nowMs = 1000;
}

FaultMask evaluate() { return rules->evaluate(in, LIM); }

void expectReport(FaultMask fault, Component comp, Symptom sym) {
  const FaultReports& r = rules->reports();
  for (int i = 0; i < r.size(); i++)
    if (r.at(i).fault == fault && r.at(i).comp == comp) {
      TEST_ASSERT_EQUAL_UINT8((uint8_t)sym, (uint8_t)r.at(i).sym);
      return;
    }
  char msg[64];
  snprintf(msg, sizeof msg, "no %s report for %s", faultName(fault), componentName(comp));
  TEST_FAIL_MESSAGE(msg);
}

}  // namespace

void setUp() {
  rules = new SafetyRules();
  healthy();
}
void tearDown() { delete rules; }

void test_healthy_car_has_no_faults() {
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  TEST_ASSERT_EQUAL_INT(0, rules->reports().size());
}

void test_radio_faults_name_the_radio() {
  in.link = LinkState::Lost;
  in.killSwitch = true;
  in.radioChannelsValid = false;
  TEST_ASSERT_EQUAL_HEX16(FAULT_LINK_LOST | FAULT_KILL_SWITCH | FAULT_INPUT, evaluate());
  expectReport(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  expectReport(FAULT_KILL_SWITCH, Component::Radio, Symptom::Active);
  expectReport(FAULT_INPUT, Component::Radio, Symptom::OutOfRange);
}

void test_degraded_link_is_not_a_fault() {
  in.link = LinkState::Degraded;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
}

void test_lost_expander_is_a_fault_only_in_kid_mode() {
  in.manualOk = false;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  in.kidRequested = true;
  TEST_ASSERT_EQUAL_HEX16(FAULT_INPUT, evaluate());
  expectReport(FAULT_INPUT, Component::Pcf, Symptom::NoAnswer);
}

void test_estop() {
  in.estop = true;
  TEST_ASSERT_EQUAL_HEX16(FAULT_ESTOP, evaluate());
  expectReport(FAULT_ESTOP, Component::Estop, Symptom::Active);
}

void test_battery_sensor_and_cutoff() {
  in.batteryVolts = 2.0f;
  TEST_ASSERT_EQUAL_HEX16(FAULT_BATTERY_SENSOR, evaluate());
  expectReport(FAULT_BATTERY_SENSOR, Component::Battery, Symptom::OutOfRange);

  delete rules;
  rules = new SafetyRules();
  in.batteryVolts = cfg::BATTERY_CUTOFF_V - 0.5f;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  in.nowMs += cfg::BATTERY_CUTOFF_HOLD_MS;
  TEST_ASSERT_EQUAL_HEX16(FAULT_BATTERY_LOW, evaluate());
  expectReport(FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit);
  TEST_ASSERT_EQUAL(BatteryState::Cutoff, rules->battery());
}

void test_stale_readings_name_the_part() {
  current.fresh[1] = false;
  current.fresh[CurrentSense::STEER] = false;
  current.fresh[CurrentSense::BUS] = false;
  TEST_ASSERT_EQUAL_HEX16(FAULT_CURRENT_SENSE, evaluate());
  expectReport(FAULT_CURRENT_SENSE, Component::FR, Symptom::Stale);
  expectReport(FAULT_CURRENT_SENSE, Component::Steer, Symptom::Stale);
  expectReport(FAULT_CURRENT_SENSE, Component::Bus, Symptom::Stale);
  TEST_ASSERT_EQUAL_INT(3, rules->reports().size());
}

void test_unsensed_channels_are_not_checked() {
  // The bench firmware senses one wheel only.
  in.wheelsSensed = 1;
  in.steerSensed = in.busSensed = false;
  for (int i = 1; i < 6; i++) current.fresh[i] = false;
  current.driverFault[2] = current.driverFault[CurrentSense::STEER] = true;
  current.amps[3] = 100.0f;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  in.nowMs += cfg::OVERCURRENT_HOLD_MS + cfg::DRIVER_FAULT_HOLD_MS;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
}

void test_no_current_sense_at_all() {
  in.current = nullptr;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
}

void test_overcurrent_per_wheel_after_hold() {
  current.amps[3] = cfg::MOTOR_CURRENT_LIMIT_A + 3.0f;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  in.nowMs += cfg::OVERCURRENT_HOLD_MS - 1;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
  in.nowMs += 1;
  TEST_ASSERT_EQUAL_HEX16(FAULT_OVERCURRENT, evaluate());
  expectReport(FAULT_OVERCURRENT, Component::RR, Symptom::OverLimit);
  const FaultReport& r = rules->reports().at(0);
  TEST_ASSERT_EQUAL_FLOAT(cfg::MOTOR_CURRENT_LIMIT_A + 3.0f, r.value);
  TEST_ASSERT_EQUAL_FLOAT(cfg::MOTOR_CURRENT_LIMIT_A, r.limit);
}

void test_overcurrent_exactly_at_limit_is_allowed() {
  current.amps[0] = cfg::MOTOR_CURRENT_LIMIT_A;
  current.driveAmps = cfg::TOTAL_CURRENT_LIMIT_A;
  evaluate();
  in.nowMs += 10 * cfg::OVERCURRENT_HOLD_MS;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
}

void test_total_overcurrent_names_all_wheels() {
  current.driveAmps = cfg::TOTAL_CURRENT_LIMIT_A + 1.0f;
  evaluate();
  in.nowMs += cfg::OVERCURRENT_HOLD_MS;
  TEST_ASSERT_EQUAL_HEX16(FAULT_OVERCURRENT, evaluate());
  expectReport(FAULT_OVERCURRENT, Component::Wheels, Symptom::OverLimit);
}

void test_driver_fault_after_hold_on_wheel_and_steering() {
  current.driverFault[0] = true;
  current.driverFault[CurrentSense::STEER] = true;
  evaluate();
  in.nowMs += cfg::DRIVER_FAULT_HOLD_MS;
  TEST_ASSERT_EQUAL_HEX16(FAULT_DRIVER, evaluate());
  expectReport(FAULT_DRIVER, Component::FL, Symptom::StuckHigh);
  expectReport(FAULT_DRIVER, Component::Steer, Symptom::StuckHigh);
}

void test_contactor_checks() {
  in.busMissing = true;
  in.busWaiting = true;
  TEST_ASSERT_EQUAL_HEX16(FAULT_NO_BUS, evaluate());
  expectReport(FAULT_NO_BUS, Component::K1, Symptom::Waiting);
  in.busWaiting = false;
  evaluate();
  expectReport(FAULT_NO_BUS, Component::K1, Symptom::Missing);
  in.busMissing = false;
  in.k1Welded = true;
  TEST_ASSERT_EQUAL_HEX16(FAULT_K1_WELDED, evaluate());
  expectReport(FAULT_K1_WELDED, Component::K1, Symptom::Welded);
  in.contactorChecked = false;  // no bus sense: flags ignored
  TEST_ASSERT_EQUAL_HEX16(FAULT_NONE, evaluate());
}

void test_box_and_tilt() {
  in.boxHot = true;
  in.boxCelsius = 70.0f;
  TEST_ASSERT_EQUAL_HEX16(FAULT_OVERTEMP, evaluate());
  expectReport(FAULT_OVERTEMP, Component::Box, Symptom::OverLimit);
  in.boxSensorLost = true;
  evaluate();
  expectReport(FAULT_OVERTEMP, Component::Box, Symptom::Stale);
  in.boxHot = false;
  in.tilted = true;
  TEST_ASSERT_EQUAL_HEX16(FAULT_TILT, evaluate());
  expectReport(FAULT_TILT, Component::Imu, Symptom::OverLimit);
}

// Every mode, every latched-fault class, stopped or moving, weld check passed or not.
void test_kill_line_truth_table() {
  const Mode MODES[] = {Mode::Locked, Mode::Kid, Mode::Parent, Mode::Killed};
  const FaultMask LATCHED[] = {FAULT_NONE, FAULT_LINK_LOST, FAULT_TILT, FAULT_ESTOP, FAULT_OVERCURRENT,
                               FAULT_K1_WELDED, FAULT_NO_BUS};
  int rows = 0;
  for (Mode m : MODES)
    for (FaultMask f : LATCHED)
      for (int stopped = 0; stopped < 2; stopped++)
        for (int allowed = 0; allowed < 2; allowed++) {
          FaultLatch latch;
          latch.raise(f);
          KillLineState k = killLines(m, latch, stopped, allowed);
          bool hard = (f & HARD_FAULTS) != 0;
          bool cut = m == Mode::Killed || hard;
          bool driving = m == Mode::Kid || m == Mode::Parent;
          char msg[80];
          snprintf(msg, sizeof msg, "%s %s stopped=%d allowed=%d", modeName(m), faultName(f), stopped, allowed);
          TEST_ASSERT_EQUAL_MESSAGE(cut, k.cut, msg);
          // Drive enable: never with a cut; on while driving or still braking.
          TEST_ASSERT_EQUAL_MESSAGE(!cut && (driving || !stopped), k.drive, msg);
          // Contactor: only with the weld check passed and no contactor fault.
          TEST_ASSERT_EQUAL_MESSAGE(allowed && !(f & CONTACTOR_FAULTS), k.contactor, msg);
          rows++;
        }
  TEST_ASSERT_EQUAL_INT(4 * 7 * 2 * 2, rows);
}

void test_fault_classes_are_consistent() {
  // Every fault that opens K1 is also a hard fault, so the drive enable is
  // already LOW when K1 opens: opening it under load would arc the contacts.
  TEST_ASSERT_EQUAL_HEX16(CONTACTOR_FAULTS & ~HARD_FAULTS, 0);
  // Permanent faults are hard faults.
  TEST_ASSERT_EQUAL_HEX16(PERMANENT_FAULTS & ~HARD_FAULTS, 0);
  // Link loss brakes to a stop: it must not be a hard fault (coasting).
  TEST_ASSERT_EQUAL_HEX16(0, HARD_FAULTS & FAULT_LINK_LOST);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_healthy_car_has_no_faults);
  RUN_TEST(test_radio_faults_name_the_radio);
  RUN_TEST(test_degraded_link_is_not_a_fault);
  RUN_TEST(test_lost_expander_is_a_fault_only_in_kid_mode);
  RUN_TEST(test_estop);
  RUN_TEST(test_battery_sensor_and_cutoff);
  RUN_TEST(test_stale_readings_name_the_part);
  RUN_TEST(test_unsensed_channels_are_not_checked);
  RUN_TEST(test_no_current_sense_at_all);
  RUN_TEST(test_overcurrent_per_wheel_after_hold);
  RUN_TEST(test_overcurrent_exactly_at_limit_is_allowed);
  RUN_TEST(test_total_overcurrent_names_all_wheels);
  RUN_TEST(test_driver_fault_after_hold_on_wheel_and_steering);
  RUN_TEST(test_contactor_checks);
  RUN_TEST(test_box_and_tilt);
  RUN_TEST(test_kill_line_truth_table);
  RUN_TEST(test_fault_classes_are_consistent);
  return UNITY_END();
}
