// The crash recorder and the serial console's parsing and text.
#include <string.h>
#include <unity.h>

#include "Console.h"
#include "CrashRing.h"
#include "EventLog.h"
#include "ModeMachine.h"

using namespace rideon;

namespace {

CrashRing ring;
CrashRecorder rec;
SavedCrash saved;
FaultReports reports;

FreezeFrame frame(uint32_t tMs, Mode mode, int16_t output = 0) {
  FreezeFrame f = {};
  f.boot = 7;
  f.tMs = tMs;
  f.mode = (uint8_t)mode;
  f.output = output;
  f.boxC = -128;
  return f;
}

// `ms` of ticks at 100 Hz, from `start`, in `mode`.
uint32_t drive(uint32_t start, uint32_t ms, Mode mode) {
  for (uint32_t t = start; t < start + ms; t += 10) {
    reports.clear();
    TEST_ASSERT_FALSE(rec.update(ring, frame(t, mode, 50), reports, saved));
  }
  return start + ms;
}

}  // namespace

void setUp() {
  memset((void*)&ring, 0xA5, sizeof(ring));  // what RTC memory holds after a power-on
  rec = CrashRecorder();
  memset(&saved, 0, sizeof(saved));
  reports.clear();
}
void tearDown() {}

// ---- CrashRing ------------------------------------------------------------------

void test_garbage_ring_is_not_intact() { TEST_ASSERT_FALSE(ring.intact()); }

void test_ring_keeps_the_last_samples_oldest_first() {
  ring.reset(3);
  TEST_ASSERT_TRUE(ring.intact());
  for (uint32_t i = 0; i < CrashRing::SAMPLES + 10; i++) ring.push(frame(i * 100, Mode::Parent));
  TEST_ASSERT_TRUE(ring.intact());
  TEST_ASSERT_EQUAL_INT(CrashRing::SAMPLES, ring.size());
  TEST_ASSERT_EQUAL_UINT32(1000, ring.at(0).tMs);
  TEST_ASSERT_EQUAL_UINT32((CrashRing::SAMPLES + 9) * 100, ring.at(CrashRing::SAMPLES - 1).tMs);
  TEST_ASSERT_EQUAL_UINT16(3, ring.boot());
}

void test_corrupted_ring_is_caught() {
  ring.reset(3);
  ring.push(frame(100, Mode::Kid));
  ((uint8_t*)&ring)[20] ^= 0x10;  // one bit flipped in a sample
  TEST_ASSERT_FALSE(ring.intact());
}

// ---- CrashRecorder ----------------------------------------------------------------

void test_samples_every_100ms() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  drive(0, 1000, Mode::Parent);
  TEST_ASSERT_EQUAL_INT(10, ring.size());
  TEST_ASSERT_EQUAL_UINT32(900, ring.at(9).tMs);
}

void test_power_on_never_keeps_the_old_ring() {
  TEST_ASSERT_FALSE(rec.begin(ring, 7, ResetReason::PowerOn, saved));
  TEST_ASSERT_TRUE(ring.intact());
  TEST_ASSERT_EQUAL_INT(0, ring.size());
}

void test_watchdog_reset_while_driving_keeps_the_ring() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  drive(0, 3000, Mode::Kid);
  // Reset: RTC memory survives, a new CrashRecorder starts.
  rec = CrashRecorder();
  TEST_ASSERT_TRUE(rec.begin(ring, 8, ResetReason::Watchdog, saved));
  TEST_ASSERT_EQUAL_UINT8(CRASH_RESET, saved.cause);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)ResetReason::Watchdog, saved.code);
  TEST_ASSERT_EQUAL_UINT16(7, saved.boot);
  TEST_ASSERT_EQUAL_UINT8(30, saved.count);
  TEST_ASSERT_EQUAL_UINT32(2900, saved.samples[29].tMs);
  // And the ring restarts for the new boot.
  TEST_ASSERT_EQUAL_INT(0, ring.size());
  TEST_ASSERT_EQUAL_UINT16(8, ring.boot());
}

void test_reset_while_parked_or_with_a_corrupt_ring_keeps_nothing() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  drive(0, 2000, Mode::Locked);
  TEST_ASSERT_FALSE(rec.begin(ring, 8, ResetReason::Panic, saved));

  rec.begin(ring, 8, ResetReason::PowerOn, saved);
  drive(0, 2000, Mode::Parent);
  ((uint8_t*)&ring)[30] ^= 1;
  TEST_ASSERT_FALSE(rec.begin(ring, 9, ResetReason::Brownout, saved));
}

void test_software_reset_keeps_nothing() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  drive(0, 2000, Mode::Parent);
  TEST_ASSERT_FALSE(rec.begin(ring, 8, ResetReason::Software, saved));
}

void test_hard_fault_while_driving_freezes_with_the_trip_last() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  uint32_t t = drive(0, 6000, Mode::Parent);
  reports.clear();
  reports.add(FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 25.0f, 20.0f);
  TEST_ASSERT_TRUE(rec.update(ring, frame(t + 5, Mode::Locked), reports, saved));
  TEST_ASSERT_EQUAL_UINT8(CRASH_FAULT, saved.cause);
  TEST_ASSERT_EQUAL_UINT8(faultBit(FAULT_OVERCURRENT), saved.code);
  TEST_ASSERT_EQUAL_UINT8(CrashRing::SAMPLES, saved.count);
  TEST_ASSERT_EQUAL_UINT32(t + 5, saved.samples[CrashRing::SAMPLES - 1].tMs);  // the trip itself
  // Still active next tick: not a new trip.
  TEST_ASSERT_FALSE(rec.update(ring, frame(t + 15, Mode::Locked), reports, saved));
}

void test_kill_switch_weld_wait_and_soft_faults_do_not_freeze() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  uint32_t t = drive(0, 2000, Mode::Parent);
  reports.clear();
  reports.add(FAULT_KILL_SWITCH, Component::Radio, Symptom::Active);
  reports.add(FAULT_NO_BUS, Component::K1, Symptom::Waiting);
  reports.add(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  reports.add(FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit);
  TEST_ASSERT_FALSE(rec.update(ring, frame(t, Mode::Killed), reports, saved));
}

void test_hard_fault_while_parked_does_not_freeze() {
  rec.begin(ring, 7, ResetReason::PowerOn, saved);
  uint32_t t = drive(0, 6000, Mode::Locked);
  reports.clear();
  reports.add(FAULT_K1_WELDED, Component::K1, Symptom::Welded);
  TEST_ASSERT_FALSE(rec.update(ring, frame(t, Mode::Locked), reports, saved));
}

void test_crash_saved_record_text() {
  LogRecord r = makeRecord(Event::CrashSaved, Severity::Warn, 9, 1500, (uint8_t)Mode::Locked, FAULT_OVERCURRENT);
  r.code = CRASH_FAULT;
  r.v[0] = faultBit(FAULT_OVERCURRENT);
  r.v[1] = 50;
  r.v[2] = 9;
  char line[160];
  formatRecord(r, line, sizeof line);
  TEST_ASSERT_NOT_NULL(strstr(line, "CRASH_SAVED cause=FAULT OVERCUR samples=50 crash_boot=9"));
  r.code = CRASH_RESET;
  r.v[0] = (int16_t)ResetReason::Watchdog;
  formatRecord(r, line, sizeof line);
  TEST_ASSERT_NOT_NULL(strstr(line, "cause=RESET WATCHDOG"));
}

// ---- Console ------------------------------------------------------------------------

void test_parse_commands() {
  TEST_ASSERT_EQUAL(Command::None, parseCommand("").cmd);
  TEST_ASSERT_EQUAL(Command::None, parseCommand("   ").cmd);
  TEST_ASSERT_EQUAL(Command::Help, parseCommand("help").cmd);
  TEST_ASSERT_EQUAL(Command::Help, parseCommand("?").cmd);
  TEST_ASSERT_EQUAL(Command::Status, parseCommand("  STATUS ").cmd);
  TEST_ASSERT_EQUAL(Command::Dtc, parseCommand("dtc").cmd);
  TEST_ASSERT_EQUAL(Command::Crash, parseCommand("Crash").cmd);
  TEST_ASSERT_EQUAL(Command::Report, parseCommand("report").cmd);
  ParsedCommand log = parseCommand("log");
  TEST_ASSERT_EQUAL(Command::Log, log.cmd);
  TEST_ASSERT_EQUAL_UINT32(LOG_DEFAULT_COUNT, log.arg);
  log = parseCommand("log 200");
  TEST_ASSERT_EQUAL(Command::Log, log.cmd);
  TEST_ASSERT_EQUAL_UINT32(200, log.arg);
  TEST_ASSERT_EQUAL_UINT32(100000, parseCommand("log 9999999").arg);
  TEST_ASSERT_EQUAL(Command::ClearDtcAsk, parseCommand("clear dtc").cmd);
  TEST_ASSERT_EQUAL(Command::ClearDtc, parseCommand("clear  DTC yes").cmd);
  TEST_ASSERT_EQUAL(Command::TestWatchdog, parseCommand("test watchdog").cmd);
}

void test_parse_rejects_anything_else() {
  const char* bad[] = {"log 0", "log x", "log 5 6", "clear dtc no", "clear dtc yes now", "clear", "status now",
                       "arm", "drive", "dtc 1", "test", "test watchdog now", "helpme", "report all"};
  for (const char* b : bad) TEST_ASSERT_EQUAL_MESSAGE(Command::Unknown, parseCommand(b).cmd, b);
}

void test_line_reader() {
  LineReader lr;
  const char* typed = "loq\bg 5\r\n";
  int lines = 0;
  for (const char* c = typed; *c; c++)
    if (lr.feed(*c)) lines++;
  TEST_ASSERT_EQUAL_INT(1, lines);  // CRLF is one line end
  TEST_ASSERT_EQUAL_STRING("log 5", lr.line());
  // Too long: cut, never overflows.
  for (int i = 0; i < 100; i++) lr.feed('a');
  TEST_ASSERT_TRUE(lr.feed('\n'));
  TEST_ASSERT_EQUAL_size_t(LineReader::MAX, strlen(lr.line()));
  // Control characters are ignored.
  lr.feed('\x1b');
  lr.feed('x');
  TEST_ASSERT_TRUE(lr.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("x", lr.line());
}

void test_fault_list() {
  char out[64];
  faultList(FAULT_NONE, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("none", out);
  faultList(FAULT_LINK_LOST | FAULT_OVERCURRENT, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("LINK,OVERCUR", out);
  char tiny[6];
  faultList(FAULT_LINK_LOST | FAULT_OVERCURRENT, tiny, sizeof tiny);
  TEST_ASSERT_EQUAL_size_t(5, strlen(tiny));  // cut, NUL-terminated
}

void test_fault_row_text() {
  FaultRow row = {};
  row.faultBit = faultBit(FAULT_OVERCURRENT);
  row.comp = (uint8_t)Component::FL;
  row.sym = (uint8_t)Symptom::OverLimit;
  row.status = DTC_FAILING_NOW | DTC_CONFIRMED;
  row.count = 3;
  row.worst = 2500;
  row.limit = 2000;
  row.first.boot = 12;
  row.first.tMs = 31200;
  row.last.boot = 40;
  row.last.tMs = 5000;
  char out[160];
  formatFaultRow(row, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("OVERCUR FL x3 OVER worst=25.00 lim=20.00 first=b12+31.2s last=b40+5.0s now", out);
}

void test_freeze_frame_text() {
  FreezeFrame f = frame(12300, Mode::Kid, 55);
  f.boot = 40;
  f.linkQuality = 95;
  f.command = 60;
  f.pedal = 60;
  f.batteryV = 2410;
  f.busV = 2390;
  f.amps[0] = 410;
  f.boxC = 41;
  char out[200];
  formatFreezeFrame(f, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING(
      "b40 +12.300s KID lq=95 cmd=0.60 out=0.55 pedal=0.60 bat=24.10V bus=23.90V I=4.10,0.00,0.00,0.00,0.00A box=41C",
      out);
  f.boxC = -128;
  formatFreezeFrame(f, out, sizeof out);
  TEST_ASSERT_NOT_NULL(strstr(out, "box=?"));
}

// SR-31
void test_report_trigger_fires_once_after_a_parked_hold() {
  ReportTrigger r;
  TEST_ASSERT_FALSE(r.update(true, true, 1000));
  TEST_ASSERT_FALSE(r.update(true, true, 3999));
  TEST_ASSERT_TRUE(r.update(true, true, 4000));
  // Held on: no second report until released and held again.
  TEST_ASSERT_FALSE(r.update(true, true, 9000));
  TEST_ASSERT_FALSE(r.update(false, true, 9010));
  TEST_ASSERT_FALSE(r.update(true, true, 9020));
  TEST_ASSERT_TRUE(r.update(true, true, 12020));
}

// SR-31
void test_report_trigger_needs_the_car_parked_and_a_steady_hold() {
  ReportTrigger r;
  // Short taps never add up.
  for (uint32_t t = 0; t < 10000; t += 1000) {
    TEST_ASSERT_FALSE(r.update(true, true, t));
    TEST_ASSERT_FALSE(r.update(false, true, t + 500));
  }
  // Honking while driving, then stopping with the horn still held: no report.
  TEST_ASSERT_FALSE(r.update(true, false, 20000));
  TEST_ASSERT_FALSE(r.update(true, true, 20010));
  TEST_ASSERT_FALSE(r.update(true, true, 30000));
  // Armed or moving during the hold cancels it.
  TEST_ASSERT_FALSE(r.update(false, true, 30010));
  TEST_ASSERT_FALSE(r.update(true, true, 31000));
  TEST_ASSERT_FALSE(r.update(true, false, 33000));
  TEST_ASSERT_FALSE(r.update(true, true, 34500));
  TEST_ASSERT_FALSE(r.update(true, true, 40000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_garbage_ring_is_not_intact);
  RUN_TEST(test_ring_keeps_the_last_samples_oldest_first);
  RUN_TEST(test_corrupted_ring_is_caught);
  RUN_TEST(test_samples_every_100ms);
  RUN_TEST(test_power_on_never_keeps_the_old_ring);
  RUN_TEST(test_watchdog_reset_while_driving_keeps_the_ring);
  RUN_TEST(test_reset_while_parked_or_with_a_corrupt_ring_keeps_nothing);
  RUN_TEST(test_software_reset_keeps_nothing);
  RUN_TEST(test_hard_fault_while_driving_freezes_with_the_trip_last);
  RUN_TEST(test_kill_switch_weld_wait_and_soft_faults_do_not_freeze);
  RUN_TEST(test_hard_fault_while_parked_does_not_freeze);
  RUN_TEST(test_crash_saved_record_text);
  RUN_TEST(test_parse_commands);
  RUN_TEST(test_parse_rejects_anything_else);
  RUN_TEST(test_line_reader);
  RUN_TEST(test_fault_list);
  RUN_TEST(test_fault_row_text);
  RUN_TEST(test_freeze_frame_text);
  RUN_TEST(test_report_trigger_fires_once_after_a_parked_hold);
  RUN_TEST(test_report_trigger_needs_the_car_parked_and_a_steady_hold);
  return UNITY_END();
}
