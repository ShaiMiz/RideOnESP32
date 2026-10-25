// Contactor checks against the motor-bus voltage: the boot weld check and a
// bus that does not come up.
#include <unity.h>

#include "ContactorCheck.h"
#include "Faults.h"

using namespace rideon;

static const ContactorParams CP = {0.3f, 0.8f, 10000, 300};
static const float BAT = 25.0f;

void setUp() {}
void tearDown() {}

void test_closing_waits_for_a_drained_bus() {
  ContactorCheck c;
  c.update(true, 20.0f, BAT, false, 0, CP);  // caps still charged from a quick key cycle
  TEST_ASSERT_FALSE(c.allowClose());
  TEST_ASSERT_TRUE(c.busMissing());  // blocks arming while K1 is held open
  c.update(true, 7.0f, BAT, false, 6000, CP);  // 28 % of the battery
  TEST_ASSERT_TRUE(c.allowClose());
  TEST_ASSERT_FALSE(c.welded());
  TEST_ASSERT_FALSE(c.busMissing());
}

void test_bus_still_live_after_timeout_is_welded() {
  ContactorCheck c;
  c.update(true, 25.0f, BAT, false, 0, CP);
  c.update(true, 25.0f, BAT, false, 9999, CP);
  TEST_ASSERT_FALSE(c.welded());
  c.update(true, 25.0f, BAT, false, 10000, CP);
  TEST_ASSERT_TRUE(c.welded());
  TEST_ASSERT_FALSE(c.allowClose());
  c.update(true, 0.0f, BAT, false, 20000, CP);  // never undone without a reboot
  TEST_ASSERT_TRUE(c.welded());
}

void test_invalid_readings_never_pass_or_fail() {
  ContactorCheck c;
  c.update(false, 0.0f, 0.0f, false, 0, CP);
  c.update(false, 0.0f, 0.0f, false, 20000, CP);
  TEST_ASSERT_FALSE(c.allowClose());
  TEST_ASSERT_FALSE(c.welded());
  TEST_ASSERT_TRUE(c.busMissing());
}

void test_bus_that_does_not_come_up() {
  ContactorCheck c;
  c.update(true, 0.0f, BAT, false, 0, CP);
  TEST_ASSERT_TRUE(c.allowClose());
  c.update(true, 0.0f, BAT, true, 10, CP);  // K1 commanded closed
  c.update(true, 0.0f, BAT, true, 309, CP);
  TEST_ASSERT_FALSE(c.busMissing());  // still within the close time
  c.update(true, 0.0f, BAT, true, 310, CP);
  TEST_ASSERT_TRUE(c.busMissing());
  c.update(true, 24.5f, BAT, true, 320, CP);  // coil fixed, bus up
  TEST_ASSERT_FALSE(c.busMissing());
}

void test_open_contactor_is_not_a_missing_bus() {
  ContactorCheck c;
  c.update(true, 0.0f, BAT, false, 0, CP);
  c.update(true, 24.5f, BAT, true, 100, CP);
  c.update(true, 0.0f, BAT, false, 1000, CP);  // opened on a fault: low bus expected
  c.update(true, 0.0f, BAT, false, 5000, CP);
  TEST_ASSERT_FALSE(c.busMissing());
  TEST_ASSERT_FALSE(c.welded());
}

void test_live_bus_at_runtime_with_k1_open_is_not_welded() {
  // A pushed car charges the bus through the motors; only the boot check judges a weld.
  ContactorCheck c;
  c.update(true, 0.0f, BAT, false, 0, CP);
  c.update(true, 24.0f, BAT, false, 30000, CP);
  TEST_ASSERT_FALSE(c.welded());
  TEST_ASSERT_TRUE(c.allowClose());
}

void test_weld_fault_is_permanent_and_opens_k1() {
  FaultLatch f;
  f.update(FAULT_K1_WELDED);
  f.update(FAULT_NONE);
  TEST_ASSERT_FALSE(f.clearInactive());
  TEST_ASSERT_TRUE(f.has(CONTACTOR_FAULTS));
  TEST_ASSERT_TRUE(f.has(HARD_FAULTS));
}

void test_driver_and_overcurrent_open_k1() {
  TEST_ASSERT_TRUE((CONTACTOR_FAULTS & FAULT_DRIVER) != 0);
  TEST_ASSERT_TRUE((CONTACTOR_FAULTS & FAULT_OVERCURRENT) != 0);
  TEST_ASSERT_TRUE((HARD_FAULTS & FAULT_NO_BUS) != 0);
  TEST_ASSERT_TRUE((CONTACTOR_FAULTS & FAULT_NO_BUS) == 0);  // K1 must stay commanded closed for NO_BUS to clear
  TEST_ASSERT_TRUE((PERMANENT_FAULTS & FAULT_NO_BUS) == 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_closing_waits_for_a_drained_bus);
  RUN_TEST(test_bus_still_live_after_timeout_is_welded);
  RUN_TEST(test_invalid_readings_never_pass_or_fail);
  RUN_TEST(test_bus_that_does_not_come_up);
  RUN_TEST(test_open_contactor_is_not_a_missing_bus);
  RUN_TEST(test_live_bus_at_runtime_with_k1_open_is_not_welded);
  RUN_TEST(test_weld_fault_is_permanent_and_opens_k1);
  RUN_TEST(test_driver_and_overcurrent_open_k1);
  return UNITY_END();
}
