// The crash recorder: the last 5 s of driving, 10 samples a second.
//
// Like a car's event data recorder (49 CFR 563 keeps speed, throttle and
// brake for the seconds before a crash), it holds freeze frames of the car
// so that after a hard fault or an unexpected reset you can see what led up
// to it. The car firmware keeps it in RTC memory, which survives a panic or
// watchdog reset but not a power cut, and saves a copy to NVS.
//
// The ring checks itself with a magic number and a CRC, so after a reset it
// is trusted only if it is intact.
#pragma once

#include <stdint.h>

#include "EventLog.h"
#include "FaultReport.h"
#include "FaultTable.h"
#include "Faults.h"

namespace rideon {

// No constructor on purpose: the car keeps one in RTC memory that must keep
// its contents through a reset; reset() starts it.
// cppcheck-suppress noConstructor
class CrashRing {
 public:
  static constexpr int SAMPLES = 50;
  static constexpr uint32_t PERIOD_MS = 100;

  // Start empty for this boot.
  void reset(uint16_t boot);
  void push(const FreezeFrame& f);
  // True when the magic and CRC match: the contents are what was written.
  bool intact() const;
  int size() const { return body_.count; }
  uint16_t boot() const { return body_.boot; }
  // i = 0 is the oldest sample.
  const FreezeFrame& at(int i) const;

 private:
  uint16_t checksum() const;

#pragma pack(push, 1)
  struct Body {
    uint16_t boot;
    uint8_t head;  // next slot to write
    uint8_t count;
    FreezeFrame samples[SAMPLES];
  };
#pragma pack(pop)

  uint32_t magic_;
  Body body_;     // what the CRC covers
  uint16_t crc_;
};

// A crash ring frozen and kept: after a hard fault, or the one found in RTC
// memory after an unexpected reset.
#pragma pack(push, 1)
struct SavedCrash {
  uint8_t format;  // CrashRing layout version (FaultTable::FORMAT)
  uint8_t cause;   // 0: reset (reason in `code`), 1: hard fault (fault bit in `code`)
  uint8_t code;
  uint16_t boot;   // the boot it happened in
  uint8_t count;
  FreezeFrame samples[CrashRing::SAMPLES];
};
#pragma pack(pop)

enum CrashCause : uint8_t { CRASH_RESET = 0, CRASH_FAULT = 1 };

void freeze(const CrashRing& ring, uint8_t cause, uint8_t code, SavedCrash& out);

// Decides when the crash ring is sampled and when it is frozen and kept.
//
// A ring is kept only if the car was driving (Kid or Parent) at some point
// in it, so a fault at boot or while parked never replaces the record of a
// real incident. It is kept
//   - at boot, after a PANIC, WATCHDOG or BROWNOUT reset, if the ring from
//     before the reset is intact;
//   - on a new hard fault (the drive was cut), except the kill switch, which
//     is the parent stopping the car on purpose, and the boot weld check
//     still waiting for the bus.
class CrashRecorder {
 public:
  // Call once at boot with the ring as found after the reset. Returns true
  // and fills `out` when it must be kept. The ring then restarts for `boot`.
  bool begin(CrashRing& ring, uint16_t boot, ResetReason reason, SavedCrash& out);
  // Call every tick with the car's freeze frame (boot, tMs and mode filled
  // in) and this tick's fault reports. Returns true and fills `out` when a
  // new hard fault froze the ring.
  bool update(CrashRing& ring, const FreezeFrame& frame, const FaultReports& reports, SavedCrash& out);

 private:
  static bool drove(const CrashRing& ring);

  bool sampled_ = false;
  uint32_t lastSampleMs_ = 0;
  FaultMask hardBefore_ = FAULT_NONE;
};

}  // namespace rideon
