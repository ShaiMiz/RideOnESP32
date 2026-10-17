// Arming, kill switch and mode-change rules.
#include <unity.h>

#include "ModeMachine.h"

using namespace rideon;

static ModeMachine mm;
static FaultLatch faults;

void setUp() {
  mm = ModeMachine();
  faults = FaultLatch();
}
void tearDown() {}

static Mode step(ModeRequest req, bool armingOk = true, bool kill = false) {
  return mm.update({kill, req, armingOk}, faults);
}

static void arm(ModeRequest to) {
  step(ModeRequest::Locked);
  step(to);
}

void test_boots_locked_and_will_not_drive_without_switch_cycle() {
  // Switch already in Kid at power-on: must stay Locked.
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
  TEST_ASSERT_TRUE(mm.needsSwitchCycle());
  step(ModeRequest::Locked);
  TEST_ASSERT_EQUAL(Mode::Kid, step(ModeRequest::Kid));
}

void test_will_not_arm_with_stick_or_pedal_held() {
  step(ModeRequest::Locked);
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Parent, false));
  TEST_ASSERT_EQUAL(Mode::Parent, step(ModeRequest::Parent, true));
}

void test_kill_switch_wins_and_needs_full_rearm() {
  arm(ModeRequest::Parent);
  TEST_ASSERT_EQUAL(Mode::Killed, step(ModeRequest::Parent, true, true));
  // Kill released, switch still at Parent: stays Locked.
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Parent));
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Parent));
  arm(ModeRequest::Parent);
  TEST_ASSERT_EQUAL(Mode::Parent, mm.mode());
}

void test_fault_locks_and_does_not_rearm_while_present() {
  arm(ModeRequest::Kid);
  faults.update(FAULT_LINK_LOST);
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
  // Condition still active: cycling the switch does not clear it.
  step(ModeRequest::Locked);
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
  TEST_ASSERT_TRUE(faults.has(FAULT_LINK_LOST));
}

void test_fault_that_goes_away_stays_latched_until_rearm() {
  arm(ModeRequest::Parent);
  faults.update(FAULT_OVERCURRENT);
  step(ModeRequest::Parent);
  faults.update(FAULT_NONE);  // current back to normal
  // Throttle still held at Parent: must not restart.
  for (int i = 0; i < 10; i++) TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Parent));
  // Switch to Locked with stick held: still latched.
  step(ModeRequest::Locked, false);
  TEST_ASSERT_TRUE(faults.any());
  // Switch to Locked, sticks centred: cleared, then may drive.
  step(ModeRequest::Locked, true);
  TEST_ASSERT_FALSE(faults.any());
  TEST_ASSERT_EQUAL(Mode::Parent, step(ModeRequest::Parent));
}

void test_setup_fault_never_clears() {
  faults.raise(FAULT_SETUP);
  for (int i = 0; i < 3; i++) {
    step(ModeRequest::Locked);
    TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
  }
  TEST_ASSERT_TRUE(faults.has(FAULT_SETUP));
}

void test_invalid_mode_switch_locks() {
  arm(ModeRequest::Kid);
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Invalid));
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid));
}

void test_kid_to_parent_needs_centred_sticks() {
  arm(ModeRequest::Kid);
  TEST_ASSERT_EQUAL(Mode::Parent, step(ModeRequest::Parent, true));
  TEST_ASSERT_EQUAL(Mode::Locked, step(ModeRequest::Kid, false));
  TEST_ASSERT_EQUAL(Mode::Kid, step(ModeRequest::Kid, true));
}

void test_switch_decoding() {
  TEST_ASSERT_EQUAL(ModeRequest::Locked, modeRequestFromSwitch(0));
  TEST_ASSERT_EQUAL(ModeRequest::Kid, modeRequestFromSwitch(1));
  TEST_ASSERT_EQUAL(ModeRequest::Parent, modeRequestFromSwitch(2));
  TEST_ASSERT_EQUAL(ModeRequest::Invalid, modeRequestFromSwitch(-1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boots_locked_and_will_not_drive_without_switch_cycle);
  RUN_TEST(test_will_not_arm_with_stick_or_pedal_held);
  RUN_TEST(test_kill_switch_wins_and_needs_full_rearm);
  RUN_TEST(test_fault_locks_and_does_not_rearm_while_present);
  RUN_TEST(test_fault_that_goes_away_stays_latched_until_rearm);
  RUN_TEST(test_setup_fault_never_clears);
  RUN_TEST(test_invalid_mode_switch_locks);
  RUN_TEST(test_kid_to_parent_needs_centred_sticks);
  RUN_TEST(test_switch_decoding);
  return UNITY_END();
}
