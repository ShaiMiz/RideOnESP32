// Pin map rules. The same checks run as static_asserts in pins.h; here they
// name the offending pin, and the bad maps below prove each rule catches what
// it is meant to catch.
#include <stdio.h>
#include <unity.h>

#include "pins.h"

using namespace pinrules;

void setUp() {}
void tearDown() {}

#define N(m) (sizeof(m) / sizeof((m)[0]))

static void expectRule(Rule rule, const char* what) {
  int i = firstBreaking(pins::MAP, pins::MAP_SIZE, rule);
  char msg[96];
  snprintf(msg, sizeof msg, "%s: %s on GPIO%d", what, i < 0 ? "" : pins::MAP[i].name, i < 0 ? -1 : pins::MAP[i].gpio);
  TEST_ASSERT_EQUAL_INT_MESSAGE(-1, i, msg);
}

// ---- The real map

void test_no_gpio_used_twice() {
  int i = firstDuplicate(pins::MAP, pins::MAP_SIZE);
  TEST_ASSERT_EQUAL_INT_MESSAGE(-1, i, i < 0 ? "" : pins::MAP[i].name);
}

void test_every_pin_is_on_the_header() { expectRule(onBoard, "not usable on the 30-pin board"); }
void test_outputs_can_drive() { expectRule(canDrive, "output on input-only pin"); }
void test_analog_on_adc1() { expectRule(analogOnAdc1, "analog input off ADC1"); }
void test_pulses_avoid_glitch_pins() { expectRule(pulseNotOnGlitchPin, "pulse input on GPIO36/39"); }
void test_kill_lines_quiet_at_boot() { expectRule(killLineQuietAtBoot, "kill line not LOW at boot"); }
void test_strapping_pins_match() { expectRule(strappingUseMatches, "strapping pin conflict"); }

void test_every_header_gpio_accounted_for() {
  // 25 header GPIOs minus the USB serial pair = 23. If one is freed, this
  // test fails on purpose so PINOUT.md's pin budget gets updated too.
  TEST_ASSERT_EQUAL_INT(23, (int)pins::MAP_SIZE);
}

void test_pwm_fits_ledc() { TEST_ASSERT_LESS_OR_EQUAL_INT(board::PWM_CHANNELS, countUse(pins::MAP, pins::MAP_SIZE, Use::Pwm)); }

void test_i2c_addresses_unique() { TEST_ASSERT_EQUAL_INT(-1, firstRepeat(i2c::ALL, sizeof(i2c::ALL))); }
void test_pcf_bits_unique() { TEST_ASSERT_EQUAL_INT(-1, firstRepeat(pcf::ALL, sizeof(pcf::ALL))); }

// ---- Bad maps: each rule must catch its mistake

void test_catches_duplicate() {
  const PinUse m[] = {{25, "A", Use::Pwm}, {26, "B", Use::Pwm}, {25, "C", Use::Pwm}};
  TEST_ASSERT_EQUAL_INT(2, firstDuplicate(m, N(m)));
}

void test_catches_missing_and_reserved_pins() {
  const PinUse m[] = {{0, "BOOT", Use::Pwm}, {9, "FLASH", Use::Pwm}, {1, "TX0", Use::UartTx}, {37, "NC", Use::Analog}};
  for (size_t i = 0; i < N(m); i++) TEST_ASSERT_FALSE_MESSAGE(onBoard(m[i]), m[i].name);
}

void test_catches_output_on_input_only() {
  TEST_ASSERT_FALSE(canDrive({34, "X", Use::Pwm}));
  TEST_ASSERT_FALSE(canDrive({39, "X", Use::KillLine}));
  TEST_ASSERT_TRUE(canDrive({34, "X", Use::Pulse}));
}

void test_catches_analog_on_adc2() {
  TEST_ASSERT_FALSE(analogOnAdc1({25, "X", Use::Analog}));
  TEST_ASSERT_TRUE(analogOnAdc1({35, "X", Use::Analog}));
}

void test_catches_pulse_on_glitch_pin() {
  TEST_ASSERT_FALSE(pulseNotOnGlitchPin({36, "X", Use::Pulse}));
  TEST_ASSERT_TRUE(pulseNotOnGlitchPin({36, "X", Use::Analog}));
}

void test_catches_kill_line_on_noisy_pin() {
  TEST_ASSERT_FALSE(killLineQuietAtBoot({14, "X", Use::KillLine}));  // toggles at boot
  TEST_ASSERT_FALSE(killLineQuietAtBoot({15, "X", Use::KillLine}));  // HIGH at boot
  TEST_ASSERT_FALSE(killLineQuietAtBoot({5, "X", Use::KillLine}));   // HIGH at boot
  TEST_ASSERT_TRUE(killLineQuietAtBoot({12, "X", Use::KillLine}));
}

void test_catches_strapping_misuse() {
  TEST_ASSERT_FALSE(strappingUseMatches({12, "X", Use::Pwm}));
  TEST_ASSERT_FALSE(strappingUseMatches({2, "X", Use::UartTx}));
  TEST_ASSERT_FALSE(strappingUseMatches({15, "X", Use::KillLine}));
  TEST_ASSERT_TRUE(strappingUseMatches({5, "X", Use::UartTx}));
}

void test_catches_repeated_address() {
  const uint8_t a[] = {0x48, 0x3C, 0x48};
  TEST_ASSERT_EQUAL_INT(2, firstRepeat(a, sizeof a));
}

void test_bad_map_fails_overall() {
  const PinUse m[] = {{25, "A", Use::Pwm}, {34, "B", Use::Pwm}};
  TEST_ASSERT_FALSE(allValid(m, N(m)));
  TEST_ASSERT_TRUE(allValid(pins::MAP, pins::MAP_SIZE));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_no_gpio_used_twice);
  RUN_TEST(test_every_pin_is_on_the_header);
  RUN_TEST(test_outputs_can_drive);
  RUN_TEST(test_analog_on_adc1);
  RUN_TEST(test_pulses_avoid_glitch_pins);
  RUN_TEST(test_kill_lines_quiet_at_boot);
  RUN_TEST(test_strapping_pins_match);
  RUN_TEST(test_every_header_gpio_accounted_for);
  RUN_TEST(test_pwm_fits_ledc);
  RUN_TEST(test_i2c_addresses_unique);
  RUN_TEST(test_pcf_bits_unique);
  RUN_TEST(test_catches_duplicate);
  RUN_TEST(test_catches_missing_and_reserved_pins);
  RUN_TEST(test_catches_output_on_input_only);
  RUN_TEST(test_catches_analog_on_adc2);
  RUN_TEST(test_catches_pulse_on_glitch_pin);
  RUN_TEST(test_catches_kill_line_on_noisy_pin);
  RUN_TEST(test_catches_strapping_misuse);
  RUN_TEST(test_catches_repeated_address);
  RUN_TEST(test_bad_map_fails_overall);
  return UNITY_END();
}
