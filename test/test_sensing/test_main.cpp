// ADS1115 register values and stale-reading detection.
#include <unity.h>

#include "Ads1115.h"

using namespace rideon;

void setUp() {}
void tearDown() {}

void test_single_shot_config_matches_datasheet() {
  // OS=1, MUX=100 (AIN0), PGA=001 (4.096 V), MODE=1, DR=111 (860 SPS), COMP_QUE=11
  TEST_ASSERT_EQUAL_HEX16(0xC3E3, ads1115::singleShotConfig(0));
  TEST_ASSERT_EQUAL_HEX16(0xD3E3, ads1115::singleShotConfig(1));
  TEST_ASSERT_EQUAL_HEX16(0xE3E3, ads1115::singleShotConfig(2));
  TEST_ASSERT_EQUAL_HEX16(0xF3E3, ads1115::singleShotConfig(3));
}

void test_conversion_done_bit() {
  TEST_ASSERT_FALSE(ads1115::conversionDone(0x43E3));
  TEST_ASSERT_TRUE(ads1115::conversionDone(0xC3E3));
}

void test_raw_to_volts() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, ads1115::rawToVolts(0));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.3f, ads1115::rawToVolts(26400));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0959f, ads1115::rawToVolts(32767));
  TEST_ASSERT_TRUE(ads1115::rawToVolts(-100) < 0.0f);
}

void test_reading_stale() {
  TEST_ASSERT_FALSE(readingStale(1000, 1250, 250));
  TEST_ASSERT_TRUE(readingStale(1000, 1251, 250));
  TEST_ASSERT_FALSE(readingStale(0xFFFFFFF0u, 0x00000010u, 250));  // millis() wrap
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_single_shot_config_matches_datasheet);
  RUN_TEST(test_conversion_done_bit);
  RUN_TEST(test_raw_to_volts);
  RUN_TEST(test_reading_stale);
  return UNITY_END();
}
