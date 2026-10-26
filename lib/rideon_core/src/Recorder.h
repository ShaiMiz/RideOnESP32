// Turns what the car is doing into log records and fault-table rows.
//
// Called once per loop after the faults and the mode are decided. It only
// compares this loop with the last one and pushes records into a RecordQueue
// (no I/O), so it costs the control loop a few microseconds. A record is made
// when something changes: a fault trips or goes away on a part, a re-arm
// clears faults, the mode, link or battery state changes. A snapshot follows
// every SNAPSHOT_MS while nothing else happens.
//
// Two conditions are normal at boot and are not counted as trips: the radio
// link before it has first come up, and the motor bus while the boot weld
// check is still waiting (Symptom::Waiting).
#pragma once

#include <stdint.h>

#include "BatteryCheck.h"
#include "EventLog.h"
#include "FaultReport.h"
#include "FaultTable.h"
#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

namespace rideon {

struct RecorderInputs {
  uint32_t nowMs;
  Mode mode;
  LinkState link;
  uint8_t linkQuality;
  BatteryState battery;
  float batteryVolts;
  float driveAmps;
  float output;  // ramped drive output, -1..1
  const FaultLatch* faults;
  const FaultReports* reports;
  FreezeFrame frame;  // boot, tMs and mode are filled in by the recorder
};

class Recorder {
 public:
  static constexpr uint32_t SNAPSHOT_MS = 60000;

  // The boot record. Call once, before the first update().
  void begin(uint16_t boot, ResetReason reason, uint32_t nowMs, RecordQueue& q);
  // A part that failed to start. Counted in the fault table as FAULT_SETUP.
  void setupFailed(Component comp, uint32_t nowMs, RecordQueue& q, FaultTable& table);
  void update(const RecorderInputs& in, RecordQueue& q, FaultTable& table);

  uint16_t boot() const { return boot_; }

 private:
  LogRecord record(Event e, Severity s, uint32_t nowMs) const;
  bool counts(const FaultReport& r) const;

  uint16_t boot_ = 0;
  Mode mode_ = Mode::Locked;
  LinkState link_ = LinkState::Lost;
  BatteryState battery_ = BatteryState::Ok;
  FaultMask latched_ = FAULT_NONE;
  bool linkSeenOk_ = false;
  uint32_t lastRecordMs_ = 0;
  FaultReports prev_;
  uint32_t since_[FaultReports::CAPACITY] = {};
};

}  // namespace rideon
