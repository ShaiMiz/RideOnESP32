// Ramps, braking and direction changes.
#include <unity.h>

#include "Pid.h"
#include "Ramp.h"

using namespace rideon;

static const RampParams RP = {0.5f, 2.0f, 0.01f};
static const float DT = 0.01f;  // 100 Hz loop

void setUp() {}
void tearDown() {}

static float run(Ramp& r, float target, int steps) {
  for (int i = 0; i < steps; i++) r.step(target, DT, RP);
  return r.value();
}

void test_accelerates_gently() {
  Ramp r;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, run(r, 1.0f, 100));  // 1 s -> half speed
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, run(r, 1.0f, 100));
}

void test_brakes_firmly() {
  Ramp r;
  run(r, 1.0f, 300);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, run(r, 0.0f, 50));  // 0.5 s full to stop
}

void test_no_reverse_until_stopped() {
  Ramp r;
  run(r, 0.5f, 200);
  // Full reverse requested at speed: value must pass through zero, never jump.
  float prev = r.value();
  bool sawZero = false;
  for (int i = 0; i < 200; i++) {
    float v = r.step(-1.0f, DT, RP);
    TEST_ASSERT_TRUE(prev - v <= 2.0f * DT + 1e-6f);
    if (v == 0.0f) sawZero = true;
    if (v < 0.0f) TEST_ASSERT_TRUE(sawZero);
    prev = v;
  }
  TEST_ASSERT_TRUE(r.value() < 0.0f);
}

void test_reverse_cap() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, limitCommand(1.0f, 0.6f, 0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.3f, limitCommand(-1.0f, 0.6f, 0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, limitCommand(3.0f, 1.0f, 0.5f));
}

void test_pid_and_slew() {
  PidParams pp = {2.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f};
  Pid pid;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.4f, pid.update(0.2f, 0.0f, DT, pp));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, pid.update(5.0f, 0.0f, DT, pp));  // clamped
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, slewToward(0.0f, 1.0f, 10.0f, DT));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accelerates_gently);
  RUN_TEST(test_brakes_firmly);
  RUN_TEST(test_no_reverse_until_stopped);
  RUN_TEST(test_reverse_cap);
  RUN_TEST(test_pid_and_slew);
  return UNITY_END();
}
