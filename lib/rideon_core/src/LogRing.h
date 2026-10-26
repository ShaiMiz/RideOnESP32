// The event log on flash: a ring of 32-byte records over whole 4 KB sectors.
//
// Flash can only be written where it was erased, and erasing a sector stalls
// both ESP32 cores for tens of ms (up to a few hundred on some chips). So the
// ring writes records only into sectors erased ahead of time, and erases only
// when the caller says it is safe (car Locked and stopped). While driving,
// writing a record is a single 32-byte program.
//
// After a power cut the newest record is found by its sequence number. A
// record torn by the cut fails its CRC and is skipped, never read as real.
// When the ring wraps, the oldest sector (128 records) is erased for reuse.
//
// The flash itself is behind FlashDevice, so the ring is tested on the PC.
#pragma once

#include <stdint.h>

#include "EventLog.h"

namespace rideon {

class FlashDevice {
 public:
  virtual ~FlashDevice() = default;
  virtual uint32_t size() const = 0;  // bytes, a whole number of sectors
  virtual bool read(uint32_t offset, void* dst, uint32_t len) = 0;
  virtual bool write(uint32_t offset, const void* src, uint32_t len) = 0;
  virtual bool eraseSector(uint32_t offset) = 0;
};

class LogRing {
 public:
  static constexpr uint32_t SECTOR = 4096;
  static constexpr uint32_t PER_SECTOR = SECTOR / sizeof(LogRecord);

  // Scan the flash: find the newest record and which sectors are erased.
  // Returns false if the device is unusable (too small, read errors).
  bool mount(FlashDevice& dev);

  // Store a record: sets its seq and CRC. Returns false when no erased space
  // is left (keep it queued and call maintain() once erasing is safe), or on
  // a write error.
  bool append(LogRecord& r);

  // Erase at most one sector, if fewer than `target` sectors ahead of the
  // write position are erased. Call only when a stall is safe. Returns true
  // if it erased.
  bool maintain(uint32_t target);

  // Records that can be written before an erase is needed.
  uint32_t room() const;
  bool mounted() const { return dev_ != nullptr; }
  uint32_t capacity() const { return slots_; }
  // Intact records found at mount plus those appended since.
  uint32_t count() const { return count_; }
  uint32_t nextSeq() const { return nextSeq_; }
  uint32_t tornAtMount() const { return torn_; }

  // The k-th newest intact record (0 = newest). Skips torn and blank slots.
  bool newest(uint32_t k, LogRecord& out);

 private:
  static bool blank(const LogRecord& r);
  uint32_t sectors() const { return slots_ / PER_SECTOR; }
  bool sectorErased(uint32_t s);

  FlashDevice* dev_ = nullptr;
  uint32_t slots_ = 0;
  uint32_t head_ = 0;  // next slot to write
  uint32_t nextSeq_ = 0;
  uint32_t count_ = 0;
  uint32_t torn_ = 0;
  // Erased slots from head_ onward. head_ + writable_ always ends on a
  // sector boundary, so the next erase is that whole sector.
  uint32_t writable_ = 0;
};

}  // namespace rideon
