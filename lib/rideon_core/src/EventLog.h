// Event log records: what happened, when, on which part, and how bad.
//
// Every record is 32 bytes with a CRC, so the same bytes can go to serial,
// to flash and to a phone, and a record torn by a power cut is detected
// instead of read as real. The event names, their severities and what their
// values mean are listed in docs/firmware/LOG_EVENTS.md; tools/check_log_events.py
// fails CI when the two disagree.
//
// Timestamps are a boot number plus milliseconds since that boot. Both are
// always right; the ESP32 has no clock that knows the date.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Faults.h"

namespace rideon {

// RFC 5424 severity numbers, so a record reads the same as a syslog line.
enum class Severity : uint8_t {
  Crit = 2,    // hard fault: drivers cut at once
  Err = 3,     // fault that brakes the car to a stop
  Warn = 4,    // degraded but still driving: weak link, low battery, records dropped
  Notice = 5,  // normal but important: boot, mode change, re-arm
  Info = 6,    // periodic snapshot; the first to be dropped
};

enum class Category : uint8_t { Sys, Safety, Mode, Link, Power, Drive, Steer, Sense, Cfg };

// The part a record is about. A fault code plus a component is one row of
// the fault table.
enum class Component : uint8_t {
  None,
  FL,
  FR,
  RL,
  RR,
  Steer,
  Wheels,  // all four drive wheels together (total current)
  Bus,     // motor bus after the contactor
  K1,      // main contactor
  Battery,
  Radio,
  Estop,
  Pcf,  // PCF8574 switch expander (pedal switches, E-stop sense)
  Pca,  // PCA9685 PWM board
  Ads,  // ADS1115 current and bus sense
  Box,  // box temperature
  Imu,
  Count
};

// How a part failed.
enum class Symptom : uint8_t {
  None,
  OverLimit,   // above its limit for longer than the hold time
  UnderLimit,  // below its limit for longer than the hold time
  Stale,       // no fresh reading
  StuckHigh,   // a driver IS line pinned high
  OutOfRange,  // reading or channel value impossible
  NoAnswer,    // device not answering on I2C
  Lost,        // radio link lost
  Active,      // switch or button in its stop position
  Waiting,     // boot check still running, not a failure yet
  Welded,
  Missing,     // expected but not there (motor bus with K1 closed)
  Count
};

enum class Event : uint8_t {
  Boot,        // code: ResetReason, v0: boot number low 16 bits
  SetupFail,   // comp: the part that did not start
  Fault,       // code: fault bit, comp, sym, v0: value x100, v1: limit x100
  FaultGone,   // code: fault bit, comp, v0: seconds it lasted x10
  Rearm,       // v0: the faults the re-arm cleared (mask)
  ModeChange,  // v0: from, v1: to
  LinkChange,  // code: LinkState, v0: link quality %
  Battery,     // code: BatteryState, v0: volts x100
  Snapshot,    // v0: battery volts x100, v1: drive amps x100, v2: output x100, v3: link quality %
  Dropped,     // v0: records dropped since the last Dropped record
  TableFull,   // a new fault row did not fit in the fault table
  LogMount,    // code: 1 if the log area was formatted, v0: records found, v1: torn slots, v2: erased sectors ready
  CrashDump,   // v0, v1: program counter of the crash, high and low 16 bits
  Count
};

enum class ResetReason : uint8_t { PowerOn, Software, Panic, Watchdog, Brownout, Other };

#pragma pack(push, 1)
struct LogRecord {
  uint32_t seq;     // record number; set when the record is stored
  uint16_t boot;    // boot counter
  uint32_t tMs;     // milliseconds since this boot
  uint8_t sevCat;   // severity in the top 3 bits, category in the low 5
  uint8_t event;    // Event
  uint8_t code;     // event-specific: fault bit, link state, reset reason
  uint8_t comp;     // Component
  uint8_t sym;      // Symptom
  uint8_t mode;     // Mode at the time
  uint8_t flags;    // reserved for flash storage (wall clock valid, freeze frame follows)
  uint16_t faults;  // latched fault mask at the time
  int16_t v[4];     // values, meaning per event (see LOG_EVENTS.md)
  uint8_t reserved[3];
  uint16_t crc;     // CRC-16/CCITT over every byte before it
};
#pragma pack(pop)
static_assert(sizeof(LogRecord) == 32, "a log record is 32 bytes on flash");

inline Severity severityOf(const LogRecord& r) { return (Severity)(r.sevCat >> 5); }
inline Category categoryOf(const LogRecord& r) { return (Category)(r.sevCat & 0x1F); }

// A record with everything but seq and crc filled in. The category follows
// from the event.
LogRecord makeRecord(Event e, Severity s, uint16_t boot, uint32_t tMs, uint8_t mode, FaultMask faults);

// Scale a value into a record's int16, clamped instead of wrapped.
int16_t scaled(float value, float scale);

uint16_t crc16(const uint8_t* data, size_t len);
// Set seq and the CRC. Call once the record's place in the log is known.
void seal(LogRecord& r, uint32_t seq);
bool intact(const LogRecord& r);

// Bit number of the lowest fault in a mask (FAULT_OVERCURRENT -> 4), or 0xFF for none.
uint8_t faultBit(FaultMask f);

const char* severityName(Severity s);
const char* categoryName(Category c);
const char* componentName(Component c);
const char* symptomName(Symptom s);
const char* eventName(Event e);
const char* resetReasonName(ResetReason r);

// One line of text, the same on serial and on the phone:
//   #4211 b42 +123.456s CRIT SAFETY FAULT OVERCUR comp=FL sym=OVER v=23.40 lim=20.00 mode=KID
// Returns the length written (always NUL-terminated, cut to fit).
size_t formatRecord(const LogRecord& r, char* out, size_t outLen);

// Records waiting to be written. The control loop only pushes here (no I/O),
// and the writer drains it. When full, the least severe record goes first
// (newest first among equals), so a burst of snapshots can never push out a
// fault. Every drop is counted.
class RecordQueue {
 public:
  static constexpr int CAPACITY = 48;

  // Returns false if a record had to be dropped (this one or a less severe one).
  bool push(const LogRecord& r);
  bool pop(LogRecord& out);
  int size() const { return count_; }
  // Drops since the last call; resets the count.
  uint32_t takeDropped();

 private:
  LogRecord items_[CAPACITY] = {};
  int head_ = 0;  // oldest
  int count_ = 0;
  uint32_t dropped_ = 0;
};

}  // namespace rideon
