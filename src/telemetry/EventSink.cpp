#include "EventSink.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_core_dump.h>
#include <esp_partition.h>

#include "config.h"
#include "safety/Watchdog.h"
#include "telemetry/Log.h"

using namespace rideon;

namespace events {

namespace {

constexpr uint8_t EVLOG_SUBTYPE = 0x40;  // partitions.csv

// The `evlog` partition as a FlashDevice.
class PartitionFlash : public FlashDevice {
 public:
  bool open() {
    part_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)EVLOG_SUBTYPE, "evlog");
    return part_ != nullptr;
  }
  uint32_t size() const override { return part_ ? part_->size : 0; }
  bool read(uint32_t offset, void* dst, uint32_t len) override {
    return esp_partition_read(part_, offset, dst, len) == ESP_OK;
  }
  bool write(uint32_t offset, const void* src, uint32_t len) override {
    return esp_partition_write(part_, offset, src, len) == ESP_OK;
  }
  bool eraseSector(uint32_t offset) override {
    return esp_partition_erase_range(part_, offset, LogRing::SECTOR) == ESP_OK;
  }
  bool eraseAll() { return esp_partition_erase_range(part_, 0, part_->size) == ESP_OK; }

 private:
  const esp_partition_t* part_ = nullptr;
};

RecordQueue queue;
FaultTable table;
Recorder recorder;
PartitionFlash flash;
LogRing ring;
uint32_t serialSeq = 0;  // used only when the flash log is not available

void loadFaultTable(Preferences& prefs) {
  static FaultRow rows[FaultTable::CAPACITY];
  if (prefs.getUChar("dtcv", 0) != FaultTable::FORMAT) return;  // none yet, or an old layout
  size_t len = prefs.getBytes("dtc", rows, sizeof(rows));
  table.restore(rows, (int)(len / sizeof(FaultRow)));
}

void saveFaultTable() {
  Preferences prefs;
  if (!prefs.begin("evlog", false)) return;
  bool ok = prefs.putBytes("dtc", table.rows(), table.size() * sizeof(FaultRow)) == table.size() * sizeof(FaultRow) ||
            table.size() == 0;
  ok &= prefs.putUChar("dtcv", FaultTable::FORMAT) == 1;
  prefs.end();
  if (!ok) logx::line("fault table NOT saved to NVS");
}

void mountFlashLog() {
  uint16_t torn = 0;
  bool formatted = false;
  if (!flash.open() || !ring.mount(flash)) {
    logx::line("evlog partition missing or unreadable: log goes to serial only. Flash with partitions.csv");
    return;
  }
  // Nothing readable but old bytes (the first boot after the partition table
  // changed): start the log area clean.
  if (ring.count() == 0 && ring.tornAtMount() > 0) {
    torn = ring.tornAtMount() > 0xFFFF ? 0xFFFF : (uint16_t)ring.tornAtMount();
    formatted = flash.eraseAll() && ring.mount(flash);
  } else {
    torn = ring.tornAtMount() > 0xFFFF ? 0xFFFF : (uint16_t)ring.tornAtMount();
  }
  while (ring.maintain(cfg::LOG_READY_SECTORS)) {
  }
  LogRecord r = makeRecord(Event::LogMount, torn ? Severity::Warn : Severity::Notice, recorder.boot(), millis(),
                           (uint8_t)Mode::Locked, FAULT_NONE);
  r.code = formatted ? 1 : 0;
  r.v[0] = (int16_t)(ring.count() > 0x7FFF ? 0x7FFF : ring.count());
  r.v[1] = (int16_t)torn;
  r.v[2] = (int16_t)(ring.room() / LogRing::PER_SECTOR);
  queue.push(r);
}

void logCrashDump(ResetReason reason) {
  if (reason != ResetReason::Panic && reason != ResetReason::Watchdog) return;  // only a fresh dump
  esp_core_dump_summary_t summary;
  if (esp_core_dump_get_summary(&summary) != ESP_OK) return;
  LogRecord r = makeRecord(Event::CrashDump, Severity::Err, recorder.boot(), millis(), (uint8_t)Mode::Locked,
                           FAULT_NONE);
  r.v[0] = (int16_t)(summary.exc_pc >> 16);
  r.v[1] = (int16_t)(summary.exc_pc & 0xFFFF);
  queue.push(r);
  logx::line("crash dump: task=%.16s pc=0x%08lX. Details: docs/firmware/LOG_EVENTS.md, Crash dumps",
             summary.exc_task, (unsigned long)summary.exc_pc);
}

}  // namespace

void begin(ResetReason reason) {
  uint16_t boot = 0;
  Preferences prefs;
  if (prefs.begin("evlog", false)) {
    boot = (uint16_t)(prefs.getUShort("boot", 0) + 1);
    prefs.putUShort("boot", boot);
    loadFaultTable(prefs);
    prefs.end();
  } else {
    logx::line("NVS not available: boot number unknown (0), fault table not loaded");
  }
  table.newBoot();
  recorder.begin(boot, reason, millis(), queue);
  mountFlashLog();
  logCrashDump(reason);
}

void setupFailed(Component comp) { recorder.setupFailed(comp, millis(), queue, table); }

void update(const RecorderInputs& in) { recorder.update(in, queue, table); }

void drain(int maxRecords, bool safeToStall) {
  if (safeToStall) {
    if (ring.mounted() && ring.room() < cfg::LOG_READY_SECTORS * LogRing::PER_SECTOR) {
      watchdog::feed();  // one sector erase: tens of ms, the car is stopped
      ring.maintain(cfg::LOG_READY_SECTORS);
    } else if (table.takeDirty()) {
      watchdog::feed();
      saveFaultTable();
    }
  }

  LogRecord r;
  char line[160];
  for (int i = 0; i < maxRecords; i++) {
    // A record waits in the queue rather than being lost to a full serial
    // buffer or a log area with no erased space.
    if (!logx::hasRoom(sizeof(line) + 2)) break;
    if (ring.mounted() && ring.room() == 0) break;
    if (!queue.pop(r)) break;
    if (ring.mounted()) {
      if (!ring.append(r)) logx::line("flash write failed: record below not stored");
    } else {
      seal(r, serialSeq++);
    }
    formatRecord(r, line, sizeof(line));
    logx::line("%s", line);
  }
}

const FaultTable& faultTable() { return table; }
LogRing& flashLog() { return ring; }
uint16_t bootNumber() { return recorder.boot(); }

}  // namespace events
