// Event log records, the record queue, the fault table and the recorder.
#include <string.h>
#include <unity.h>

#include "EventLog.h"
#include "FaultReport.h"
#include "FaultTable.h"
#include "Recorder.h"

using namespace rideon;

void setUp() {}
void tearDown() {}

namespace {

LogRecord rec(Severity s, uint32_t t = 0) { return makeRecord(Event::Snapshot, s, 1, t, 0, FAULT_NONE); }

// One loop's worth of recorder inputs with nothing wrong.
struct Car {
  FaultLatch faults;
  FaultReports reports;
  RecordQueue q;
  FaultTable table;
  Recorder recorder;
  Mode mode = Mode::Locked;
  LinkState link = LinkState::Ok;
  BatteryState battery = BatteryState::Ok;

  void tick(uint32_t now) {
    faults.update(reports.mask());
    recorder.update({now, mode, link, 95, battery, 25.0f, 3.0f, 0.2f, &faults, &reports, FreezeFrame{}}, q, table);
  }
  // Pop every queued record; return how many matched `e`, keeping the last one.
  int count(Event e, LogRecord* last = nullptr) {
    int n = 0;
    LogRecord r;
    while (q.pop(r)) {
      if (r.event == (uint8_t)e) {
        n++;
        if (last) *last = r;
      }
    }
    return n;
  }
};

}  // namespace

void test_record_is_sealed_and_torn_records_are_caught() {
  LogRecord r = makeRecord(Event::Fault, Severity::Crit, 42, 123456, 1, FAULT_OVERCURRENT);
  seal(r, 7);
  TEST_ASSERT_EQUAL_UINT32(7, r.seq);
  TEST_ASSERT_TRUE(intact(r));
  ((uint8_t*)&r)[10] ^= 0x01;  // one bit flipped, as a power cut mid-write can leave it
  TEST_ASSERT_FALSE(intact(r));
}

void test_severity_and_category_share_a_byte() {
  LogRecord r = makeRecord(Event::Boot, Severity::Notice, 1, 0, 0, FAULT_NONE);
  TEST_ASSERT_EQUAL(Severity::Notice, severityOf(r));
  TEST_ASSERT_EQUAL(Category::Sys, categoryOf(r));
  r = makeRecord(Event::Fault, Severity::Crit, 1, 0, 0, FAULT_NONE);
  TEST_ASSERT_EQUAL(Severity::Crit, severityOf(r));
  TEST_ASSERT_EQUAL(Category::Safety, categoryOf(r));
  TEST_ASSERT_EQUAL(Category::Link, categoryOf(makeRecord(Event::LinkChange, Severity::Warn, 1, 0, 0, FAULT_NONE)));
}

void test_scaled_rounds_and_clamps() {
  TEST_ASSERT_EQUAL_INT16(2340, scaled(23.4f, 100.0f));
  TEST_ASSERT_EQUAL_INT16(-150, scaled(-1.5f, 100.0f));
  TEST_ASSERT_EQUAL_INT16(32767, scaled(1000.0f, 100.0f));
  TEST_ASSERT_EQUAL_INT16(-32768, scaled(-1000.0f, 100.0f));
}

void test_fault_bit_numbers() {
  TEST_ASSERT_EQUAL_UINT8(0, faultBit(FAULT_LINK_LOST));
  TEST_ASSERT_EQUAL_UINT8(4, faultBit(FAULT_OVERCURRENT));
  TEST_ASSERT_EQUAL_UINT8(0xFF, faultBit(FAULT_NONE));
}

void test_format_fault_line() {
  LogRecord r = makeRecord(Event::Fault, Severity::Crit, 42, 123456, (uint8_t)Mode::Kid, FAULT_OVERCURRENT);
  r.code = faultBit(FAULT_OVERCURRENT);
  r.comp = (uint8_t)Component::FL;
  r.sym = (uint8_t)Symptom::OverLimit;
  r.v[0] = 2340;
  r.v[1] = 2000;
  seal(r, 4211);
  char line[160];
  formatRecord(r, line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING(
      "#4211 b42 +123.456s CRIT SAFETY FAULT OVERCUR comp=FL sym=OVER v=23.40 lim=20.00 mode=KID faults=0x0010", line);
}

void test_format_cuts_to_fit() {
  LogRecord r = makeRecord(Event::Boot, Severity::Notice, 1, 0, 0, FAULT_NONE);
  char line[12];
  size_t n = formatRecord(r, line, sizeof(line));
  TEST_ASSERT_EQUAL(11, n);
  TEST_ASSERT_EQUAL(11, strlen(line));
}

void test_queue_is_first_in_first_out() {
  RecordQueue q;
  q.push(rec(Severity::Info, 1));
  q.push(rec(Severity::Crit, 2));
  LogRecord r;
  TEST_ASSERT_TRUE(q.pop(r));
  TEST_ASSERT_EQUAL_UINT32(1, r.tMs);
  TEST_ASSERT_TRUE(q.pop(r));
  TEST_ASSERT_EQUAL_UINT32(2, r.tMs);
  TEST_ASSERT_FALSE(q.pop(r));
}

void test_full_queue_drops_least_severe_first() {
  RecordQueue q;
  for (int i = 0; i < RecordQueue::CAPACITY - 1; i++) q.push(rec(Severity::Crit, i));
  q.push(rec(Severity::Info, 1000));
  TEST_ASSERT_FALSE(q.push(rec(Severity::Err, 2000)));  // full: the snapshot makes way
  TEST_ASSERT_EQUAL(RecordQueue::CAPACITY, q.size());
  TEST_ASSERT_FALSE(q.push(rec(Severity::Info, 3000)));  // nothing less severe left: the snapshot itself goes
  TEST_ASSERT_EQUAL_UINT32(2, q.takeDropped());
  TEST_ASSERT_EQUAL_UINT32(0, q.takeDropped());
  LogRecord r;
  bool sawErr = false;
  while (q.pop(r)) {
    TEST_ASSERT_NOT_EQUAL(Severity::Info, severityOf(r));
    if (r.tMs == 2000) sawErr = true;
  }
  TEST_ASSERT_TRUE(sawErr);
}

void test_reports_never_lose_a_fault_from_the_mask() {
  // The mask stops the car; a full report list must never hide a fault.
  FaultReports r;
  for (int i = 0; i < FaultReports::CAPACITY; i++) r.add(FAULT_CURRENT_SENSE, (Component)(i % 16), Symptom::Stale);
  for (int i = 0; i < FaultReports::CAPACITY; i++) r.add(FAULT_DRIVER, (Component)(i % 16), Symptom::StuckHigh);
  r.add(FAULT_ESTOP, Component::Estop, Symptom::Active);
  TEST_ASSERT_TRUE(r.mask() & FAULT_ESTOP);
  TEST_ASSERT_EQUAL(FaultReports::CAPACITY, r.size());
  r.add(FAULT_CURRENT_SENSE, Component::FL, Symptom::None);  // same fault and part: still one report
  TEST_ASSERT_EQUAL(FaultReports::CAPACITY, r.size());
  r.clear();
  TEST_ASSERT_EQUAL(FAULT_NONE, r.mask());
}

void test_table_counts_trips_and_keeps_worst() {
  FaultTable t;
  FreezeFrame f = {};
  f.boot = 3;
  t.trip({FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 22.0f, 20.0f}, f);
  f.boot = 4;
  t.trip({FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 25.0f, 20.0f}, f);
  t.trip({FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 21.0f, 20.0f}, f);
  t.trip({FAULT_OVERCURRENT, Component::RR, Symptom::OverLimit, 21.0f, 20.0f}, f);
  TEST_ASSERT_EQUAL(2, t.size());
  const FaultRow* fl = t.find(FAULT_OVERCURRENT, Component::FL);
  TEST_ASSERT_NOT_NULL(fl);
  TEST_ASSERT_EQUAL_UINT16(3, fl->count);
  TEST_ASSERT_EQUAL_INT16(2500, fl->worst);
  TEST_ASSERT_EQUAL_UINT16(3, fl->first.boot);
  TEST_ASSERT_EQUAL_UINT16(4, fl->last.boot);
}

void test_table_worst_battery_is_the_lowest() {
  FaultTable t;
  FreezeFrame f = {};
  t.trip({FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit, 20.5f, 21.0f}, f);
  t.trip({FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit, 19.8f, 21.0f}, f);
  t.trip({FAULT_BATTERY_LOW, Component::Battery, Symptom::UnderLimit, 20.9f, 21.0f}, f);
  TEST_ASSERT_EQUAL_INT16(1980, t.find(FAULT_BATTERY_LOW, Component::Battery)->worst);
}

void test_table_status_bits() {
  FaultTable t;
  FreezeFrame f = {};
  t.trip({FAULT_DRIVER, Component::FR, Symptom::StuckHigh, 0, 0}, f);
  const FaultRow* row = t.find(FAULT_DRIVER, Component::FR);
  TEST_ASSERT_TRUE(row->status & DTC_FAILING_NOW);
  TEST_ASSERT_TRUE(row->status & DTC_CONFIRMED);
  t.gone(FAULT_DRIVER, Component::FR);
  TEST_ASSERT_FALSE(row->status & DTC_FAILING_NOW);
  TEST_ASSERT_TRUE(row->status & DTC_FAILED_THIS_BOOT);
  t.newBoot();
  TEST_ASSERT_FALSE(row->status & DTC_FAILED_THIS_BOOT);
  TEST_ASSERT_TRUE(row->status & DTC_CONFIRMED);  // never ages out on its own
  TEST_ASSERT_TRUE(t.takeDirty());
  TEST_ASSERT_FALSE(t.takeDirty());
}

void test_table_full_refuses_new_rows() {
  FaultTable t;
  FreezeFrame f = {};
  for (int i = 0; i < FaultTable::CAPACITY; i++)
    TEST_ASSERT_NOT_NULL(t.trip({(FaultMask)(1u << (i % 14)), (Component)(i / 14 + 1), Symptom::None, 0, 0}, f));
  TEST_ASSERT_NULL(t.trip({FAULT_TILT, Component::Imu, Symptom::OverLimit, 0, 0}, f));
  TEST_ASSERT_NOT_NULL(t.trip({FAULT_LINK_LOST, Component::FL, Symptom::None, 0, 0}, f));  // existing row still counts
}

void test_table_restores_after_reboot() {
  FaultTable t;
  FreezeFrame f = {};
  f.boot = 7;
  t.trip({FAULT_OVERCURRENT, Component::RL, Symptom::OverLimit, 22.0f, 20.0f}, f);
  t.trip({FAULT_DRIVER, Component::FR, Symptom::StuckHigh, 0, 0}, f);
  FaultRow saved[FaultTable::CAPACITY];
  memcpy(saved, t.rows(), sizeof(saved));
  saved[1].comp = 200;  // a corrupt row is dropped, not trusted

  FaultTable after;
  after.restore(saved, t.size());
  after.newBoot();
  TEST_ASSERT_EQUAL(1, after.size());
  const FaultRow* row = after.find(FAULT_OVERCURRENT, Component::RL);
  TEST_ASSERT_NOT_NULL(row);
  TEST_ASSERT_EQUAL_UINT16(1, row->count);
  TEST_ASSERT_EQUAL_UINT16(7, row->first.boot);
  TEST_ASSERT_FALSE(row->status & DTC_FAILING_NOW);
  TEST_ASSERT_TRUE(row->status & DTC_CONFIRMED);
  TEST_ASSERT_FALSE(after.takeDirty());
}

void test_recorder_boot_record() {
  RecordQueue q;
  Recorder r;
  r.begin(42, ResetReason::Watchdog, 5, q);
  LogRecord out;
  TEST_ASSERT_TRUE(q.pop(out));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Event::Boot, out.event);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)ResetReason::Watchdog, out.code);
  TEST_ASSERT_EQUAL_UINT16(42, out.boot);
}

void test_recorder_trip_once_then_gone() {
  Car c;
  c.recorder.begin(1, ResetReason::PowerOn, 0, c.q);
  c.tick(10);
  c.count(Event::Count);  // drop the boot and link records

  c.reports.add(FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 23.4f, 20.0f);
  c.tick(20);
  LogRecord f;
  TEST_ASSERT_EQUAL(1, c.count(Event::Fault, &f));
  TEST_ASSERT_EQUAL(Severity::Crit, severityOf(f));  // over-current cuts the drivers
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Component::FL, f.comp);
  TEST_ASSERT_EQUAL_INT16(2340, f.v[0]);

  c.tick(30);  // still over: no second record, no second count
  TEST_ASSERT_EQUAL(0, c.count(Event::Fault));
  TEST_ASSERT_EQUAL_UINT16(1, c.table.find(FAULT_OVERCURRENT, Component::FL)->count);

  c.reports.clear();
  c.tick(1520);
  LogRecord g;
  TEST_ASSERT_EQUAL(1, c.count(Event::FaultGone, &g));
  TEST_ASSERT_EQUAL_INT16(15, g.v[0]);  // 1.5 s
  TEST_ASSERT_FALSE(c.table.find(FAULT_OVERCURRENT, Component::FL)->status & DTC_FAILING_NOW);

  c.reports.add(FAULT_OVERCURRENT, Component::FL, Symptom::OverLimit, 21.0f, 20.0f);
  c.tick(2000);
  TEST_ASSERT_EQUAL(1, c.count(Event::Fault));
  TEST_ASSERT_EQUAL_UINT16(2, c.table.find(FAULT_OVERCURRENT, Component::FL)->count);
}

void test_recorder_soft_fault_is_err() {
  Car c;
  c.reports.add(FAULT_CURRENT_SENSE, Component::RR, Symptom::Stale);
  c.tick(10);
  LogRecord f;
  TEST_ASSERT_EQUAL(1, c.count(Event::Fault, &f));
  TEST_ASSERT_EQUAL(Severity::Err, severityOf(f));
}

void test_recorder_ignores_link_before_first_contact() {
  Car c;
  c.link = LinkState::Lost;
  c.reports.add(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  c.tick(10);  // radio still off at boot: normal
  TEST_ASSERT_EQUAL(0, c.count(Event::Fault));
  TEST_ASSERT_EQUAL(0, c.table.size());

  c.link = LinkState::Ok;
  c.reports.clear();
  c.tick(20);
  c.link = LinkState::Lost;
  c.reports.add(FAULT_LINK_LOST, Component::Radio, Symptom::Lost);
  c.tick(30);  // lost after it was up: counted
  TEST_ASSERT_EQUAL(1, c.count(Event::Fault));
  TEST_ASSERT_EQUAL(1, c.table.size());
}

void test_recorder_ignores_boot_weld_wait() {
  Car c;
  c.reports.add(FAULT_NO_BUS, Component::K1, Symptom::Waiting);
  c.tick(10);
  TEST_ASSERT_EQUAL(0, c.count(Event::Fault));
  c.reports.clear();
  c.reports.add(FAULT_NO_BUS, Component::K1, Symptom::Missing);
  c.tick(20);
  TEST_ASSERT_EQUAL(1, c.count(Event::Fault));
}

void test_recorder_logs_rearm() {
  Car c;
  c.reports.add(FAULT_KILL_SWITCH, Component::Radio, Symptom::Active);
  c.tick(10);
  c.reports.clear();
  c.tick(20);
  c.faults.clearInactive();  // the ModeMachine re-arms
  c.tick(30);
  LogRecord r;
  TEST_ASSERT_EQUAL(1, c.count(Event::Rearm, &r));
  TEST_ASSERT_EQUAL_UINT16(FAULT_KILL_SWITCH, (uint16_t)r.v[0]);
}

void test_recorder_logs_mode_link_and_battery_changes() {
  Car c;
  c.tick(10);  // link Lost -> Ok
  c.count(Event::Count);
  c.mode = Mode::Kid;
  c.battery = BatteryState::Low;
  c.link = LinkState::Degraded;
  c.tick(20);
  LogRecord r;
  bool mode = false, link = false, battery = false;
  while (c.q.pop(r)) {
    if (r.event == (uint8_t)Event::ModeChange) {
      mode = true;
      TEST_ASSERT_EQUAL_INT16((int16_t)Mode::Locked, r.v[0]);
      TEST_ASSERT_EQUAL_INT16((int16_t)Mode::Kid, r.v[1]);
    }
    if (r.event == (uint8_t)Event::LinkChange) {
      link = true;
      TEST_ASSERT_EQUAL(Severity::Warn, severityOf(r));
      TEST_ASSERT_EQUAL_INT16(95, r.v[0]);
    }
    if (r.event == (uint8_t)Event::Battery) {
      battery = true;
      TEST_ASSERT_EQUAL(Severity::Warn, severityOf(r));
      TEST_ASSERT_EQUAL_INT16(2500, r.v[0]);
    }
  }
  TEST_ASSERT_TRUE(mode && link && battery);
  c.tick(30);  // nothing changed: nothing logged
  TEST_ASSERT_EQUAL(0, c.q.size());
}

void test_recorder_snapshot_when_quiet() {
  Car c;
  c.tick(0);
  c.count(Event::Count);
  c.tick(Recorder::SNAPSHOT_MS - 1);
  TEST_ASSERT_EQUAL(0, c.count(Event::Snapshot));
  c.tick(Recorder::SNAPSHOT_MS);
  LogRecord s;
  TEST_ASSERT_EQUAL(1, c.count(Event::Snapshot, &s));
  TEST_ASSERT_EQUAL(Severity::Info, severityOf(s));
  TEST_ASSERT_EQUAL_INT16(2500, s.v[0]);
}

void test_recorder_setup_failure_is_counted() {
  RecordQueue q;
  FaultTable t;
  Recorder r;
  r.setupFailed(Component::Ads, 5, q, t);
  LogRecord out;
  TEST_ASSERT_TRUE(q.pop(out));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)Event::SetupFail, out.event);
  TEST_ASSERT_EQUAL(Severity::Crit, severityOf(out));
  TEST_ASSERT_NOT_NULL(t.find(FAULT_SETUP, Component::Ads));
}

void test_recorder_reports_dropped_records() {
  Car c;
  for (int i = 0; i < RecordQueue::CAPACITY + 3; i++) c.q.push(rec(Severity::Info, i));
  c.tick(10);
  LogRecord d;
  TEST_ASSERT_EQUAL(1, c.count(Event::Dropped, &d));
  TEST_ASSERT_TRUE(d.v[0] >= 3);
}

void test_every_name_is_known() {
  for (int i = 0; i < (int)Event::Count; i++) TEST_ASSERT_NOT_EQUAL(0, strcmp("?", eventName((Event)i)));
  for (int i = 0; i < (int)Component::Count; i++) TEST_ASSERT_NOT_EQUAL(0, strcmp("?", componentName((Component)i)));
  for (int i = 0; i < (int)Symptom::Count; i++) TEST_ASSERT_NOT_EQUAL(0, strcmp("?", symptomName((Symptom)i)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_record_is_sealed_and_torn_records_are_caught);
  RUN_TEST(test_severity_and_category_share_a_byte);
  RUN_TEST(test_scaled_rounds_and_clamps);
  RUN_TEST(test_fault_bit_numbers);
  RUN_TEST(test_format_fault_line);
  RUN_TEST(test_format_cuts_to_fit);
  RUN_TEST(test_queue_is_first_in_first_out);
  RUN_TEST(test_full_queue_drops_least_severe_first);
  RUN_TEST(test_reports_never_lose_a_fault_from_the_mask);
  RUN_TEST(test_table_counts_trips_and_keeps_worst);
  RUN_TEST(test_table_worst_battery_is_the_lowest);
  RUN_TEST(test_table_status_bits);
  RUN_TEST(test_table_full_refuses_new_rows);
  RUN_TEST(test_table_restores_after_reboot);
  RUN_TEST(test_recorder_boot_record);
  RUN_TEST(test_recorder_trip_once_then_gone);
  RUN_TEST(test_recorder_soft_fault_is_err);
  RUN_TEST(test_recorder_ignores_link_before_first_contact);
  RUN_TEST(test_recorder_ignores_boot_weld_wait);
  RUN_TEST(test_recorder_logs_rearm);
  RUN_TEST(test_recorder_logs_mode_link_and_battery_changes);
  RUN_TEST(test_recorder_snapshot_when_quiet);
  RUN_TEST(test_recorder_setup_failure_is_counted);
  RUN_TEST(test_recorder_reports_dropped_records);
  RUN_TEST(test_every_name_is_known);
  return UNITY_END();
}
