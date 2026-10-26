// The fault table: one row per fault and part that has ever tripped, kept
// across reboots, like a car's diagnostic trouble codes (ISO 14229).
//
// Each row counts how often the fault tripped, when it first and last did,
// the worst value seen, and a freeze frame of the car at the first and the
// latest trip. Rows never age out on their own: they stay until someone
// clears the table on purpose, and that clear is logged.
#pragma once

#include <stdint.h>

#include "EventLog.h"
#include "FaultReport.h"

namespace rideon {

// The car at one moment, small enough to keep two per fault row.
#pragma pack(push, 1)
struct FreezeFrame {
  uint16_t boot;
  uint32_t tMs;
  uint8_t mode;
  uint8_t linkQuality;  // %
  int16_t command;      // x100, -100..100
  int16_t output;       // x100, ramped output
  int16_t pedal;        // x100
  int16_t batteryV;     // x100
  int16_t busV;         // x100
  int16_t amps[5];      // x100: FL, FR, RL, RR, steering
  int8_t boxC;          // -128 when unknown
};
#pragma pack(pop)

// Status bits, numbered as in the ISO 14229 DTC status byte.
enum : uint8_t {
  DTC_FAILING_NOW = 1u << 0,         // testFailed
  DTC_FAILED_THIS_BOOT = 1u << 1,    // testFailedThisOperationCycle
  DTC_CONFIRMED = 1u << 3,           // confirmedDTC: stored
  DTC_FAILED_SINCE_CLEAR = 1u << 5,  // testFailedSinceLastClear
};

struct FaultRow {
  uint8_t faultBit;  // bit number in the fault mask
  uint8_t comp;      // Component
  uint8_t sym;       // Symptom of the latest trip
  uint8_t status;    // DTC_* bits
  uint16_t count;    // trips since the table was cleared (stops at 65535)
  int16_t worst;     // worst value x100: highest, or lowest for an under-limit fault
  int16_t limit;     // the limit x100 at the latest trip
  FreezeFrame first;
  FreezeFrame last;
};

class FaultTable {
 public:
  static constexpr int CAPACITY = 24;

  // A trip: a fault and part that was not active last loop now is. Returns
  // the row, or nullptr when a new row does not fit (the caller logs it).
  const FaultRow* trip(const FaultReport& r, const FreezeFrame& frame);
  // The condition went away (the fault itself stays latched until a re-arm).
  void gone(FaultMask fault, Component comp);
  // At boot, after loading the table from flash: nothing is failing yet.
  void newBoot();
  void clear();

  int size() const { return count_; }
  const FaultRow& at(int i) const { return rows_[i]; }
  const FaultRow* find(FaultMask fault, Component comp) const;
  // True when a row changed since the last call (it needs writing to flash).
  bool takeDirty();

 private:
  int indexOf(uint8_t bit, uint8_t comp) const;

  FaultRow rows_[CAPACITY] = {};
  int count_ = 0;
  bool dirty_ = false;
};

}  // namespace rideon
