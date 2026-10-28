// CRSF telemetry frames to the radio and how often they are sent.
#include <math.h>
#include <string.h>
#include <unity.h>

#include "CrsfTelemetry.h"

using namespace rideon;

void setUp() {}
void tearDown() {}

static TelemetryInputs inputs(uint32_t now, Mode mode = Mode::Parent, FaultMask latched = FAULT_NONE,
                              float amps = 0.0f, bool linkUp = true) {
  return {now, linkUp, mode, latched, 25.2f, amps};
}

// Runs one tick and sends whatever is built. Returns the frame type, or 0.
static uint8_t tick(TelemetryScheduler& s, const TelemetryInputs& in, uint8_t* buf) {
  uint8_t n = s.update(in, buf);
  if (n == 0) return 0;
  s.sent();
  return buf[2];
}

void test_battery_frame_bytes() {
  // Checked against the CRSF spec and a separate CRC-8/DVB-S2 implementation.
  const uint8_t expected[] = {0xC8, 0x0A, 0x08, 0x00, 0xF6, 0x00, 0x7B, 0x00, 0x04, 0xD2, 0x00, 0x69};
  uint8_t buf[CRSF_TELEMETRY_MAX];
  TEST_ASSERT_EQUAL(sizeof(expected), crsfBatteryFrame(buf, 24.6f, 12.3f, 1234, 0));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, sizeof(expected));
}

void test_battery_frame_clamps_bad_values() {
  uint8_t buf[CRSF_TELEMETRY_MAX];
  crsfBatteryFrame(buf, -3.0f, 1e9f, 0x7FFFFFFF, 250);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);  // negative volts read as 0
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[4]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[5]);  // huge current saturates
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[6]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[7]);  // 24-bit capacity saturates
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[9]);
  TEST_ASSERT_EQUAL(100, buf[10]);
  crsfBatteryFrame(buf, NAN, 0.0f, 0, 0);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[4]);  // NaN volts read as 0
}

void test_flight_mode_frame_bytes() {
  uint8_t buf[CRSF_TELEMETRY_MAX];
  uint8_t n = crsfFlightModeFrame(buf, "LOCKED ESTOP");
  TEST_ASSERT_EQUAL(17, n);
  TEST_ASSERT_EQUAL_HEX8(0xC8, buf[0]);
  TEST_ASSERT_EQUAL(15, buf[1]);  // type + 13 text bytes + crc
  TEST_ASSERT_EQUAL_HEX8(0x21, buf[2]);
  TEST_ASSERT_EQUAL_STRING("LOCKED ESTOP", (const char*)&buf[3]);
  TEST_ASSERT_EQUAL_HEX8(0x09, buf[16]);
}

void test_flight_mode_text_is_cut_to_fit() {
  uint8_t buf[CRSF_TELEMETRY_MAX];
  uint8_t n = crsfFlightModeFrame(buf, "THIS TEXT IS FAR TOO LONG");
  TEST_ASSERT_EQUAL(CRSF_TELEMETRY_MAX, n);
  TEST_ASSERT_EQUAL(CRSF_STATUS_MAX - 1, strlen((const char*)&buf[3]));
}

void test_status_text() {
  char s[CRSF_STATUS_MAX];
  statusText(s, sizeof(s), Mode::Parent, FAULT_NONE);
  TEST_ASSERT_EQUAL_STRING("PARENT", s);
  statusText(s, sizeof(s), Mode::Locked, FAULT_ESTOP);
  TEST_ASSERT_EQUAL_STRING("LOCKED ESTOP", s);
  statusText(s, sizeof(s), Mode::Killed, (FaultMask)(FAULT_LINK_LOST | FAULT_OVERCURRENT));
  TEST_ASSERT_EQUAL_STRING("KILLED LINK+", s);
  // The longest mode and fault names still fit.
  statusText(s, sizeof(s), Mode::Locked, (FaultMask)(FAULT_BATTERY_SENSOR | FAULT_OVERTEMP));
  TEST_ASSERT_EQUAL_STRING("LOCKED BATT_SEN", s);
}

void test_status_first_then_battery_every_slot() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_FLIGHT_MODE, tick(s, inputs(1000), buf));
  // Nothing more until the next slot.
  for (uint32_t t = 1010; t < 1200; t += 10) TEST_ASSERT_EQUAL(0, tick(s, inputs(t), buf));
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_BATTERY, tick(s, inputs(1200), buf));
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_BATTERY, tick(s, inputs(1400), buf));
}

void test_at_most_five_frames_a_second() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  int frames = 0, status = 0;
  for (uint32_t t = 0; t < 10000; t += 10) {
    uint8_t type = tick(s, inputs(t), buf);
    frames += type ? 1 : 0;
    status += type == CRSF_TYPE_FLIGHT_MODE ? 1 : 0;
  }
  TEST_ASSERT_EQUAL(50, frames);
  TEST_ASSERT_EQUAL(10, status);  // repeated once a second when nothing changes
}

void test_status_change_goes_out_at_the_next_slot() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  tick(s, inputs(0), buf);
  tick(s, inputs(200), buf);
  TEST_ASSERT_EQUAL(0, tick(s, inputs(250, Mode::Locked, FAULT_ESTOP), buf));  // mid-slot
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_FLIGHT_MODE, tick(s, inputs(400, Mode::Locked, FAULT_ESTOP), buf));
  TEST_ASSERT_EQUAL_STRING("LOCKED ESTOP", (const char*)&buf[3]);
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_BATTERY, tick(s, inputs(600, Mode::Locked, FAULT_ESTOP), buf));
}

void test_unsent_frame_is_built_again() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  // The UART had no room: the status is not marked sent and comes again next tick.
  TEST_ASSERT_TRUE(s.update(inputs(0, Mode::Locked, FAULT_ESTOP), buf) > 0);
  uint8_t n = s.update(inputs(10, Mode::Locked, FAULT_ESTOP), buf);
  TEST_ASSERT_EQUAL(17, n);
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_FLIGHT_MODE, buf[2]);
  s.sent();
  TEST_ASSERT_EQUAL(0, s.update(inputs(20, Mode::Locked, FAULT_ESTOP), buf));
}

void test_nothing_without_a_link() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  for (uint32_t t = 0; t < 2000; t += 10) TEST_ASSERT_EQUAL(0, s.update(inputs(t, Mode::Killed, FAULT_LINK_LOST, 0, false), buf));
  s.sent();  // harmless with nothing built
  // Back with a link: the status goes first.
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_FLIGHT_MODE, tick(s, inputs(2000, Mode::Killed, FAULT_LINK_LOST), buf));
  TEST_ASSERT_EQUAL_STRING("KILLED LINK", (const char*)&buf[3]);
}

void test_charge_used_is_integrated_every_tick() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  // 36 A for 10 s at 100 Hz = 100 mAh, counted also while the link is down.
  for (uint32_t t = 0; t <= 10000; t += 10) s.update(inputs(t, Mode::Parent, FAULT_NONE, 36.0f, t < 5000), buf);
  TEST_ASSERT_UINT32_WITHIN(1, 100, s.usedMah());
  // Negative readings and long gaps add nothing.
  s.update(inputs(10010, Mode::Parent, FAULT_NONE, -50.0f), buf);
  s.update(inputs(20010, Mode::Parent, FAULT_NONE, 36.0f), buf);
  TEST_ASSERT_UINT32_WITHIN(1, 100, s.usedMah());
}

void test_battery_frame_carries_the_readings() {
  TelemetryScheduler s;
  uint8_t buf[CRSF_TELEMETRY_MAX];
  tick(s, inputs(0, Mode::Parent, FAULT_NONE, 12.3f), buf);
  TEST_ASSERT_EQUAL_HEX8(CRSF_TYPE_BATTERY, tick(s, inputs(200, Mode::Parent, FAULT_NONE, 12.3f), buf));
  TEST_ASSERT_EQUAL(252, (buf[3] << 8) | buf[4]);  // 25.2 V
  TEST_ASSERT_EQUAL(123, (buf[5] << 8) | buf[6]);  // 12.3 A
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_battery_frame_bytes);
  RUN_TEST(test_battery_frame_clamps_bad_values);
  RUN_TEST(test_flight_mode_frame_bytes);
  RUN_TEST(test_flight_mode_text_is_cut_to_fit);
  RUN_TEST(test_status_text);
  RUN_TEST(test_status_first_then_battery_every_slot);
  RUN_TEST(test_at_most_five_frames_a_second);
  RUN_TEST(test_status_change_goes_out_at_the_next_slot);
  RUN_TEST(test_unsent_frame_is_built_again);
  RUN_TEST(test_nothing_without_a_link);
  RUN_TEST(test_charge_used_is_integrated_every_tick);
  RUN_TEST(test_battery_frame_carries_the_readings);
  return UNITY_END();
}
