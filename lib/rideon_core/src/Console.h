// The serial console: a few read-only commands to look at the log, the
// fault table and the crash recorder over USB, plus clearing the fault table.
//
// None of them can arm, drive or change a limit. Parsing and the text of
// each line live here so they are tested on the PC; src/telemetry/Console
// does the I/O. Commands are listed in docs/firmware/LOG_EVENTS.md.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CrashRing.h"
#include "FaultTable.h"
#include "Faults.h"

namespace rideon {

enum class Command : uint8_t {
  None,          // empty line
  Help,
  Status,
  Log,           // arg: how many records, newest last
  Dtc,
  Crash,
  Report,        // status, fault table, crash and the last 50 log records, in one go
  ClearDtcAsk,   // "clear dtc" without "yes"
  ClearDtc,      // "clear dtc yes"
  TestWatchdog,  // "test watchdog": hang the loop so the watchdog resets the ESP32 (commissioning only)
  Unknown,
};

struct ParsedCommand {
  Command cmd;
  uint32_t arg;
};

constexpr uint32_t LOG_DEFAULT_COUNT = 50;

// Case and extra spaces are ignored.
ParsedCommand parseCommand(const char* line);

// Collects typed characters into a line. Backspace works; characters past
// the buffer are dropped; CR, LF or CRLF ends a line.
class LineReader {
 public:
  static constexpr size_t MAX = 40;
  // True when `c` completed a line; read it with line().
  bool feed(char c);
  const char* line() const { return done_; }

 private:
  char buf_[MAX + 1] = {};
  char done_[MAX + 1] = {};
  size_t len_ = 0;
  bool lastCr_ = false;
};

// "OVERCUR,DRIVER" for every fault in the mask, or "none".
size_t faultList(FaultMask mask, char* out, size_t outLen);

// One fault-table row:
//   OVERCUR FL x3 OVER worst=25.00 lim=20.00 first=b12+31.2s last=b40+5.0s now
size_t formatFaultRow(const FaultRow& row, char* out, size_t outLen);

// One freeze frame:
//   b40 +12.300s KID lq=95 cmd=0.60 out=0.55 pedal=0.60 bat=24.10V bus=23.90V I=4.10,4.00,3.90,4.20,0.00A box=41C
size_t formatFreezeFrame(const FreezeFrame& f, char* out, size_t outLen);

// The report trigger, for a phone on the listen-only Bluetooth module, which
// cannot type: holding the horn (the kid's button or the radio's) for HOLD_MS
// with the car parked prints one full report. It fires once per hold, and
// only if the car stayed parked for the whole hold. It only prints.
class ReportTrigger {
 public:
  static constexpr uint32_t HOLD_MS = 3000;
  // parked: Locked or Killed and stopped. True on the tick the report is due.
  bool update(bool hornHeld, bool parked, uint32_t nowMs);

 private:
  bool holding_ = false;
  bool blocked_ = false;
  uint32_t startMs_ = 0;
};

}  // namespace rideon
