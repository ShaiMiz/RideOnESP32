// Fault latching, hold timers and battery checks.
#include <unity.h>

#include "BatteryCheck.h"
#include "Faults.h"

using namespace rideon;

static const BatteryParams BP = {5.0f, 32.0f, 22.0f, 21.0f, 22.5f, 27.5f, 2000};

void setUp() {}
void tearDown() {}

void test_latch_holds_after_condition_clears() {
  FaultLatch f;
  f.update(FAULT_OVERCURRENT);
  f.update(FAULT_NONE);
  TEST_ASSERT_TRUE(f.has(FAULT_OVERCURRENT));
}

void test_clear_only_removes_inactive() {
  FaultLatch f;
  f.update(FAULT_OVERCURRENT | FAULT_LINK_LOST);
  f.update(FAULT_LINK_LOST);
  TEST_ASSERT_FALSE(f.clearInactive());
  TEST_ASSERT_TRUE(f.has(FAULT_LINK_LOST));
  TEST_ASSERT_FALSE(f.has(FAULT_OVERCURRENT));
}

void test_setup_fault_is_permanent() {
  FaultLatch f;
  f.raise(FAULT_SETUP);
  TEST_ASSERT_FALSE(f.clearInactive());
  TEST_ASSERT_TRUE(f.has(FAULT_SETUP));
}

void test_hold_timer_ignores_short_spikes() {
  HoldTimer t;
  TEST_ASSERT_FALSE(t.update(true, 0, 300));
  TEST_ASSERT_FALSE(t.update(true, 299, 300));
  TEST_ASSERT_FALSE(t.update(false, 300, 300));  // spike over
  TEST_ASSERT_FALSE(t.update(true, 400, 300));
  TEST_ASSERT_TRUE(t.update(true, 700, 300));
}

void test_battery_sensor_fault_is_not_ok() {
  BatteryCheck b;
  TEST_ASSERT_EQUAL(BatteryState::SensorFault, b.update(0.0f, 0, BP));   // wire off
  TEST_ASSERT_EQUAL(BatteryState::SensorFault, b.update(3.0f, 10, BP));
  TEST_ASSERT_EQUAL(BatteryState::SensorFault, b.update(40.0f, 20, BP));
}

void test_battery_cutoff_needs_hold_and_hysteresis() {
  BatteryCheck b;
  TEST_ASSERT_EQUAL(BatteryState::Ok, b.update(25.0f, 0, BP));
  TEST_ASSERT_EQUAL(BatteryState::Low, b.update(20.5f, 100, BP));      // sag starts
  TEST_ASSERT_EQUAL(BatteryState::Low, b.update(20.5f, 1500, BP));     // not long enough
  TEST_ASSERT_EQUAL(BatteryState::Cutoff, b.update(20.5f, 2200, BP));
  TEST_ASSERT_EQUAL(BatteryState::Cutoff, b.update(22.2f, 2300, BP));  // rebound, under recover
  TEST_ASSERT_EQUAL(BatteryState::Ok, b.update(22.6f, 2400, BP));
}

void test_battery_high_warning() {
  BatteryCheck b;
  b.update(28.0f, 0, BP);
  TEST_ASSERT_TRUE(b.highWarning());
  b.update(26.0f, 10, BP);
  TEST_ASSERT_FALSE(b.highWarning());
}

void test_fault_names() {
  TEST_ASSERT_EQUAL_STRING("OK", faultName(FAULT_NONE));
  TEST_ASSERT_EQUAL_STRING("LINK", faultName(FAULT_LINK_LOST | FAULT_DRIVER));
  TEST_ASSERT_EQUAL_STRING("DRIVER", faultName(FAULT_DRIVER));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_latch_holds_after_condition_clears);
  RUN_TEST(test_clear_only_removes_inactive);
  RUN_TEST(test_setup_fault_is_permanent);
  RUN_TEST(test_hold_timer_ignores_short_spikes);
  RUN_TEST(test_battery_sensor_fault_is_not_ok);
  RUN_TEST(test_battery_cutoff_needs_hold_and_hysteresis);
  RUN_TEST(test_battery_high_warning);
  RUN_TEST(test_fault_names);
  return UNITY_END();
}
