// Link-loss detection and radio channel decoding.
#include <unity.h>

#include "ArmingCheck.h"
#include "ChannelMap.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

using namespace rideon;

static const LinkParams LP = {200, 70, 40};
static const ChannelLimits CL = {900, 2100, 988, 1500, 2012, 30, 1300, 1700};

void setUp() {}
void tearDown() {}

static LinkState link(bool up, bool any, uint32_t last, uint8_t lq, uint32_t now) {
  return evaluateLink({up, any, last, lq, now}, LP);
}

void test_lost_before_first_packet() { TEST_ASSERT_EQUAL(LinkState::Lost, link(true, false, 0, 100, 10)); }

void test_ok_with_fresh_packets() { TEST_ASSERT_EQUAL(LinkState::Ok, link(true, true, 1000, 100, 1010)); }

void test_lost_within_200ms_even_if_flag_still_up() {
  // AlfredoCRSF keeps isLinkUp() true for 300 ms; packet age catches it sooner.
  TEST_ASSERT_EQUAL(LinkState::Ok, link(true, true, 1000, 100, 1200));
  TEST_ASSERT_EQUAL(LinkState::Lost, link(true, true, 1000, 100, 1201));
}

void test_names() {
  // These names go into the log and onto the radio screen.
  TEST_ASSERT_EQUAL_STRING("LOST", linkName(LinkState::Lost));
  TEST_ASSERT_EQUAL_STRING("WEAK", linkName(LinkState::Degraded));
  TEST_ASSERT_EQUAL_STRING("OK", linkName(LinkState::Ok));
  TEST_ASSERT_EQUAL_STRING("?", linkName((LinkState)9));
  TEST_ASSERT_EQUAL_STRING("KILLED", modeName(Mode::Killed));
  TEST_ASSERT_EQUAL_STRING("?", modeName((Mode)9));
}

void test_lost_when_flag_down() { TEST_ASSERT_EQUAL(LinkState::Lost, link(false, true, 1000, 100, 1001)); }

void test_link_quality_bands() {
  TEST_ASSERT_EQUAL(LinkState::Ok, link(true, true, 1000, 70, 1001));
  TEST_ASSERT_EQUAL(LinkState::Degraded, link(true, true, 1000, 69, 1001));
  TEST_ASSERT_EQUAL(LinkState::Degraded, link(true, true, 1000, 40, 1001));
  TEST_ASSERT_EQUAL(LinkState::Lost, link(true, true, 1000, 39, 1001));
}

void test_millis_wraparound() {
  TEST_ASSERT_EQUAL(LinkState::Ok, link(true, true, 0xFFFFFFF0u, 100, 0x00000010u));
  TEST_ASSERT_EQUAL(LinkState::Lost, link(true, true, 0xFFFFFF00u, 100, 0x00000100u));
}

void test_channels_are_one_based() {
  TEST_ASSERT_EQUAL(1, ch::STEERING);
  TEST_ASSERT_EQUAL(2, ch::THROTTLE);
  TEST_ASSERT_TRUE(ch::KILL >= 1 && ch::MODE >= 1 && ch::SPEED_CAP >= 1 && ch::HORN >= 1);
}

void test_stick_fraction() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, stickFraction(1500, CL));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, stickFraction(1525, CL));  // inside deadband
  TEST_ASSERT_EQUAL_FLOAT(1.0f, stickFraction(2012, CL));
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, stickFraction(988, CL));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, stickFraction(0, CL));     // invalid reads as stop
  TEST_ASSERT_EQUAL_FLOAT(0.0f, stickFraction(2500, CL));
}

void test_switch_positions() {
  TEST_ASSERT_EQUAL(0, switchPosition(988, CL));
  TEST_ASSERT_EQUAL(1, switchPosition(1500, CL));
  TEST_ASSERT_EQUAL(2, switchPosition(2012, CL));
  TEST_ASSERT_EQUAL(-1, switchPosition(0, CL));
}

void test_arming_check() {
  ArmingParams ap = {1500, 60, 0.05f};
  TEST_ASSERT_TRUE(armingOk({1500, 1500, 0.0f}, ap));
  TEST_ASSERT_FALSE(armingOk({1600, 1500, 0.0f}, ap));  // throttle held
  TEST_ASSERT_FALSE(armingOk({1500, 1400, 0.0f}, ap));  // steering held
  TEST_ASSERT_FALSE(armingOk({1500, 1500, 0.3f}, ap));  // pedal pressed
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lost_before_first_packet);
  RUN_TEST(test_ok_with_fresh_packets);
  RUN_TEST(test_lost_within_200ms_even_if_flag_still_up);
  RUN_TEST(test_lost_when_flag_down);
  RUN_TEST(test_link_quality_bands);
  RUN_TEST(test_millis_wraparound);
  RUN_TEST(test_channels_are_one_based);
  RUN_TEST(test_stick_fraction);
  RUN_TEST(test_switch_positions);
  RUN_TEST(test_arming_check);
  RUN_TEST(test_names);
  return UNITY_END();
}
