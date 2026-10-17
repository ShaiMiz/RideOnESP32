// CRSF RC-frame detection and resync.
#include <unity.h>

#include "CrsfFrameDetector.h"

using namespace rideon;

void setUp() {}
void tearDown() {}

// Build a frame: sync, length, type, payload, CRC. Returns its size.
static int frame(uint8_t* out, uint8_t type, uint8_t payloadLen, uint8_t fill) {
  out[0] = 0xC8;
  out[1] = (uint8_t)(payloadLen + 2);
  out[2] = type;
  for (uint8_t i = 0; i < payloadLen; i++) out[3 + i] = (uint8_t)(fill + i);
  out[3 + payloadLen] = crsfCrc8(&out[2], (uint8_t)(payloadLen + 1));
  return payloadLen + 4;
}

static int feedAll(CrsfFrameDetector& d, const uint8_t* b, int n, uint32_t now) {
  int found = 0;
  for (int i = 0; i < n; i++) found += d.feed(b[i], now) ? 1 : 0;
  return found;
}

void test_detects_rc_frame() {
  uint8_t f[64];
  int n = frame(f, 0x16, 22, 0x10);
  CrsfFrameDetector d;
  TEST_ASSERT_EQUAL(1, feedAll(d, f, n, 500));
  TEST_ASSERT_EQUAL_UINT32(1, d.frames());
  TEST_ASSERT_EQUAL_UINT32(500, d.lastFrameMs());
}

void test_ignores_other_frame_types() {
  uint8_t f[64];
  int n = frame(f, 0x14, 10, 0x01);  // link statistics
  CrsfFrameDetector d;
  TEST_ASSERT_EQUAL(0, feedAll(d, f, n, 10));
  TEST_ASSERT_EQUAL_UINT32(0, d.frames());
}

void test_rejects_bad_crc() {
  uint8_t f[64];
  int n = frame(f, 0x16, 22, 0x10);
  f[n - 1] ^= 0xFF;
  CrsfFrameDetector d;
  TEST_ASSERT_EQUAL(0, feedAll(d, f, n, 10));
}

void test_resyncs_after_false_start() {
  // A stray sync byte with a long plausible length used to swallow the next
  // real frame. Like AlfredoCRSF, the detector must still find both frames.
  uint8_t s[200];
  int n = 0;
  s[n++] = 0xC8;
  s[n++] = 48;
  n += frame(&s[n], 0x16, 22, 0x20);
  n += frame(&s[n], 0x16, 22, 0x40);
  CrsfFrameDetector d;
  feedAll(d, s, n, 10);
  TEST_ASSERT_EQUAL_UINT32(2, d.frames());
}

void test_back_to_back_frames() {
  uint8_t s[200];
  int n = frame(s, 0x16, 22, 0x01);
  n += frame(&s[n], 0x14, 10, 0x02);
  n += frame(&s[n], 0x16, 22, 0x03);
  CrsfFrameDetector d;
  TEST_ASSERT_EQUAL(2, feedAll(d, s, n, 10));
}

void test_idle_gap_drops_partial_frame() {
  uint8_t f[64];
  int n = frame(f, 0x16, 22, 0x10);
  CrsfFrameDetector d;
  feedAll(d, f, 10, 0);                              // first 10 bytes, then silence
  TEST_ASSERT_EQUAL(1, feedAll(d, f, n, 500));       // a full frame after the gap
  TEST_ASSERT_EQUAL_UINT32(1, d.frames());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_detects_rc_frame);
  RUN_TEST(test_ignores_other_frame_types);
  RUN_TEST(test_rejects_bad_crc);
  RUN_TEST(test_resyncs_after_false_start);
  RUN_TEST(test_back_to_back_frames);
  RUN_TEST(test_idle_gap_drops_partial_frame);
  return UNITY_END();
}
