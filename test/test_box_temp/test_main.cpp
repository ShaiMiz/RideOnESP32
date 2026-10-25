// DS18B20 scratchpad decoding and the box over-temperature check.
#include <unity.h>

#include "BoxTempCheck.h"
#include "Ds18b20.h"

using namespace rideon;

static const BoxTempParams OPTIONAL = {65.0f, 55.0f, 5000, 5000, false};
static const BoxTempParams REQUIRED = {65.0f, 55.0f, 5000, 5000, true};

void setUp() {}
void tearDown() {}

static void pad(uint8_t* p, int16_t raw) {
  uint8_t b[9] = {(uint8_t)(raw & 0xFF), (uint8_t)(raw >> 8), 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
  b[8] = ds18b20::crc8(b, 8);
  for (int i = 0; i < 9; i++) p[i] = b[i];
}

void test_crc8_known_value() {
  // ROM code from the Maxim application note 27 example
  const uint8_t rom[8] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xA2};
  TEST_ASSERT_EQUAL_HEX8(0xA2, ds18b20::crc8(rom, 7));
  TEST_ASSERT_EQUAL_HEX8(0x00, ds18b20::crc8(rom, 8));
}

void test_decode_datasheet_values() {
  uint8_t p[9];
  float c = 0;
  pad(p, 0x0191);  // +25.0625 C
  TEST_ASSERT_TRUE(ds18b20::decodeCelsius(p, c));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 25.0625f, c);
  pad(p, (int16_t)0xFF5E);  // -10.125 C
  TEST_ASSERT_TRUE(ds18b20::decodeCelsius(p, c));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, -10.125f, c);
}

void test_decode_rejects_bad_reads() {
  uint8_t p[9];
  float c = 0;
  pad(p, 0x0191);
  p[0] ^= 0x01;  // one bit flipped on the wire
  TEST_ASSERT_FALSE(ds18b20::decodeCelsius(p, c));
  pad(p, 0x0550);  // 85 C power-on value
  TEST_ASSERT_FALSE(ds18b20::decodeCelsius(p, c));
  uint8_t none[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // no sensor
  TEST_ASSERT_FALSE(ds18b20::decodeCelsius(none, c));
}

void test_hot_after_hold_and_until_cooled() {
  BoxTempCheck t;
  TEST_ASSERT_FALSE(t.update(true, 70.0f, 0, 0, OPTIONAL));
  TEST_ASSERT_FALSE(t.update(true, 70.0f, 4999, 4999, OPTIONAL));
  TEST_ASSERT_TRUE(t.update(true, 70.0f, 5000, 5000, OPTIONAL));
  TEST_ASSERT_TRUE(t.update(true, 60.0f, 6000, 6000, OPTIONAL));  // between recover and limit
  TEST_ASSERT_FALSE(t.update(true, 54.0f, 7000, 7000, OPTIONAL));
}

void test_short_spike_is_not_hot() {
  BoxTempCheck t;
  t.update(true, 70.0f, 0, 0, OPTIONAL);
  t.update(true, 50.0f, 1000, 1000, OPTIONAL);
  TEST_ASSERT_FALSE(t.update(true, 70.0f, 6000, 6000, OPTIONAL));
}

void test_lost_sensor_only_hot_when_required() {
  BoxTempCheck a, b;
  TEST_ASSERT_FALSE(a.update(false, 0.0f, 0, 10000, OPTIONAL));
  TEST_ASSERT_TRUE(a.sensorLost());
  TEST_ASSERT_TRUE(b.update(false, 0.0f, 0, 10000, REQUIRED));
  b.update(true, 30.0f, 10000, 10000, REQUIRED);
  TEST_ASSERT_FALSE(b.hot());
  TEST_ASSERT_TRUE(b.update(true, 30.0f, 10000, 15001, REQUIRED));  // reading went stale
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc8_known_value);
  RUN_TEST(test_decode_datasheet_values);
  RUN_TEST(test_decode_rejects_bad_reads);
  RUN_TEST(test_hot_after_hold_and_until_cooled);
  RUN_TEST(test_short_spike_is_not_hot);
  RUN_TEST(test_lost_sensor_only_hot_when_required);
  return UNITY_END();
}
