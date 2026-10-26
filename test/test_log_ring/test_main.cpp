// The flash log ring, on a RAM model of NOR flash: writes can only clear
// bits, and only an erase sets a sector back to 0xFF.
#include <string.h>
#include <unity.h>

#include <vector>

#include "LogRing.h"

using namespace rideon;

void setUp() {}
void tearDown() {}

namespace {

class FakeFlash : public FlashDevice {
 public:
  explicit FakeFlash(uint32_t sectors) : mem(sectors * LogRing::SECTOR, 0xFF) {}
  uint32_t size() const override { return mem.size(); }
  bool read(uint32_t off, void* dst, uint32_t len) override {
    if (off + len > mem.size()) return false;
    memcpy(dst, &mem[off], len);
    return true;
  }
  bool write(uint32_t off, const void* src, uint32_t len) override {
    if (failWrites || off + len > mem.size()) return false;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < len; i++) mem[off + i] &= s[i];  // NOR: program only clears bits
    writes++;
    return true;
  }
  bool eraseSector(uint32_t off) override {
    if (off % LogRing::SECTOR || off >= mem.size()) return false;
    memset(&mem[off], 0xFF, LogRing::SECTOR);
    erases++;
    return true;
  }
  std::vector<uint8_t> mem;
  bool failWrites = false;
  int writes = 0;
  int erases = 0;
};

LogRecord event(uint32_t t) { return makeRecord(Event::Snapshot, Severity::Info, 1, t, 0, FAULT_NONE); }

constexpr uint32_t PER = LogRing::PER_SECTOR;

}  // namespace

void test_fresh_flash_is_all_room() {
  FakeFlash f(4);
  LogRing ring;
  TEST_ASSERT_TRUE(ring.mount(f));
  TEST_ASSERT_EQUAL_UINT32(4 * PER, ring.capacity());
  TEST_ASSERT_EQUAL_UINT32(4 * PER, ring.room());
  TEST_ASSERT_EQUAL_UINT32(0, ring.count());
  LogRecord r;
  TEST_ASSERT_FALSE(ring.newest(0, r));
}

void test_too_small_is_refused() {
  FakeFlash f(1);
  LogRing ring;
  TEST_ASSERT_FALSE(ring.mount(f));
  LogRecord r = event(0);
  TEST_ASSERT_FALSE(ring.append(r));
}

void test_append_then_read_newest_first() {
  FakeFlash f(4);
  LogRing ring;
  ring.mount(f);
  for (uint32_t t = 0; t < 10; t++) {
    LogRecord r = event(t);
    TEST_ASSERT_TRUE(ring.append(r));
    TEST_ASSERT_EQUAL_UINT32(t, r.seq);
  }
  LogRecord r;
  TEST_ASSERT_TRUE(ring.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(9, r.tMs);
  TEST_ASSERT_TRUE(ring.newest(9, r));
  TEST_ASSERT_EQUAL_UINT32(0, r.tMs);
  TEST_ASSERT_FALSE(ring.newest(10, r));
  TEST_ASSERT_EQUAL_UINT32(4 * PER - 10, ring.room());
}

void test_survives_power_cycle() {
  FakeFlash f(4);
  {
    LogRing ring;
    ring.mount(f);
    for (uint32_t t = 0; t < 200; t++) {
      LogRecord r = event(t);
      ring.append(r);
    }
  }
  LogRing again;  // power off, power on
  TEST_ASSERT_TRUE(again.mount(f));
  TEST_ASSERT_EQUAL_UINT32(200, again.count());
  TEST_ASSERT_EQUAL_UINT32(200, again.nextSeq());
  TEST_ASSERT_EQUAL_UINT32(4 * PER - 200, again.room());
  LogRecord r = event(999);
  TEST_ASSERT_TRUE(again.append(r));
  TEST_ASSERT_EQUAL_UINT32(200, r.seq);
  TEST_ASSERT_TRUE(again.newest(1, r));
  TEST_ASSERT_EQUAL_UINT32(199, r.tMs);
}

void test_torn_record_is_skipped_and_never_read() {
  FakeFlash f(4);
  {
    LogRing ring;
    ring.mount(f);
    for (uint32_t t = 0; t < 5; t++) {
      LogRecord r = event(t);
      ring.append(r);
    }
  }
  // Power cut half way through writing record 5: only its first bytes landed.
  LogRecord half = event(5);
  seal(half, 5);
  f.write(5 * sizeof(LogRecord), &half, 12);

  LogRing ring;
  TEST_ASSERT_TRUE(ring.mount(f));
  TEST_ASSERT_EQUAL_UINT32(1, ring.tornAtMount());
  TEST_ASSERT_EQUAL_UINT32(5, ring.count());
  LogRecord r;
  TEST_ASSERT_TRUE(ring.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(4, r.tMs);
  // Writing goes on in the next sector; the torn sector tail is left alone.
  TEST_ASSERT_EQUAL_UINT32(3 * PER, ring.room());
  LogRecord next = event(6);
  TEST_ASSERT_TRUE(ring.append(next));
  TEST_ASSERT_EQUAL_UINT32(5, next.seq);
  TEST_ASSERT_TRUE(ring.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(6, r.tMs);
  TEST_ASSERT_TRUE(ring.newest(1, r));
  TEST_ASSERT_EQUAL_UINT32(4, r.tMs);
}

void test_full_ring_waits_for_an_erase() {
  FakeFlash f(4);
  LogRing ring;
  ring.mount(f);
  for (uint32_t t = 0; t < 4 * PER; t++) {
    LogRecord r = event(t);
    TEST_ASSERT_TRUE(ring.append(r));
  }
  LogRecord r = event(9999);
  TEST_ASSERT_FALSE(ring.append(r));  // no erased space: the caller keeps it queued
  TEST_ASSERT_EQUAL(0, f.erases);

  TEST_ASSERT_TRUE(ring.maintain(1));  // car Locked: erase the oldest sector
  TEST_ASSERT_EQUAL(1, f.erases);
  TEST_ASSERT_EQUAL_UINT32(PER, ring.room());
  TEST_ASSERT_EQUAL_UINT32(3 * PER, ring.count());
  TEST_ASSERT_FALSE(ring.maintain(1));  // already one sector ahead
  TEST_ASSERT_TRUE(ring.append(r));

  LogRing again;
  again.mount(f);
  TEST_ASSERT_TRUE(again.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(9999, r.tMs);
  TEST_ASSERT_TRUE(again.newest(3 * PER, r));  // the oldest left: record 128
  TEST_ASSERT_EQUAL_UINT32(PER, r.tMs);
}

void test_maintain_never_erases_the_sector_being_written() {
  FakeFlash f(2);
  LogRing ring;
  ring.mount(f);
  for (uint32_t t = 0; t < 2 * PER; t++) {
    LogRecord r = event(t);
    ring.append(r);
  }
  ring.maintain(2);  // erases sector 0 (the oldest)
  for (uint32_t t = 0; t < 3; t++) {
    LogRecord r = event(1000 + t);
    ring.append(r);
  }
  // Head is 3 records into sector 0. Sector 1 is now the oldest and may go;
  // sector 0, which holds the 3 newest records, may not.
  TEST_ASSERT_TRUE(ring.maintain(2));
  TEST_ASSERT_EQUAL_UINT32(2 * PER - 3, ring.room());
  int before = f.erases;
  TEST_ASSERT_FALSE(ring.maintain(2));
  TEST_ASSERT_EQUAL(before, f.erases);
  LogRecord r;
  TEST_ASSERT_TRUE(ring.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(1002, r.tMs);
  TEST_ASSERT_TRUE(ring.newest(2, r));
  TEST_ASSERT_EQUAL_UINT32(1000, r.tMs);
  TEST_ASSERT_EQUAL_UINT32(3, ring.count());
}

void test_wraps_many_times() {
  FakeFlash f(3);
  LogRing ring;
  ring.mount(f);
  for (uint32_t t = 0; t < 10 * PER + 7; t++) {
    LogRecord r = event(t);
    if (!ring.append(r)) {
      TEST_ASSERT_TRUE(ring.maintain(1));
      TEST_ASSERT_TRUE(ring.append(r));
    }
  }
  LogRing again;
  again.mount(f);
  TEST_ASSERT_EQUAL_UINT32(10 * PER + 7, again.nextSeq());
  LogRecord r;
  TEST_ASSERT_TRUE(again.newest(0, r));
  TEST_ASSERT_EQUAL_UINT32(10 * PER + 6, r.tMs);
  TEST_ASSERT_EQUAL_UINT32(0, again.tornAtMount());
}

void test_write_error_skips_the_slot() {
  FakeFlash f(2);
  LogRing ring;
  ring.mount(f);
  LogRecord r = event(1);
  ring.append(r);
  f.failWrites = true;
  r = event(2);
  TEST_ASSERT_FALSE(ring.append(r));
  f.failWrites = false;
  r = event(3);
  TEST_ASSERT_TRUE(ring.append(r));
  LogRecord out;
  TEST_ASSERT_TRUE(ring.newest(0, out));
  TEST_ASSERT_EQUAL_UINT32(3, out.tMs);
  TEST_ASSERT_TRUE(ring.newest(1, out));
  TEST_ASSERT_EQUAL_UINT32(1, out.tMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_flash_is_all_room);
  RUN_TEST(test_too_small_is_refused);
  RUN_TEST(test_append_then_read_newest_first);
  RUN_TEST(test_survives_power_cycle);
  RUN_TEST(test_torn_record_is_skipped_and_never_read);
  RUN_TEST(test_full_ring_waits_for_an_erase);
  RUN_TEST(test_maintain_never_erases_the_sector_being_written);
  RUN_TEST(test_wraps_many_times);
  RUN_TEST(test_write_error_skips_the_slot);
  return UNITY_END();
}
