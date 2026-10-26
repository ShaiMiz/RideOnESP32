#include "LogRing.h"

namespace rideon {

namespace {
constexpr uint32_t REC = sizeof(LogRecord);
constexpr uint32_t CHUNK = 16;  // records read per flash access at mount
}  // namespace

bool LogRing::blank(const LogRecord& r) {
  const uint8_t* p = (const uint8_t*)&r;
  for (uint32_t i = 0; i < REC; i++)
    if (p[i] != 0xFF) return false;
  return true;
}

bool LogRing::mount(FlashDevice& dev) {
  dev_ = nullptr;
  slots_ = (dev.size() / SECTOR) * PER_SECTOR;
  if (slots_ < 2 * PER_SECTOR) return false;

  bool any = false;
  uint32_t newestSlot = 0, newestSeq = 0;
  count_ = torn_ = 0;
  LogRecord buf[CHUNK];
  for (uint32_t base = 0; base < slots_; base += CHUNK) {
    if (!dev.read(base * REC, buf, sizeof(buf))) return false;
    for (uint32_t i = 0; i < CHUNK; i++) {
      const LogRecord& r = buf[i];
      if (blank(r)) continue;
      if (!intact(r)) {
        torn_++;
        continue;
      }
      count_++;
      // Newest by sequence number, compared so that a wrapped counter still works.
      if (!any || (int32_t)(r.seq - newestSeq) > 0) {
        any = true;
        newestSeq = r.seq;
        newestSlot = base + i;
      }
    }
  }
  dev_ = &dev;
  head_ = any ? (newestSlot + 1) % slots_ : 0;
  nextSeq_ = any ? newestSeq + 1 : 0;

  // Writable space: the rest of head's sector if it is all blank (otherwise
  // a torn write sits there: skip to the next sector), then whole erased
  // sectors after it. A sector is never reused while it still holds the
  // records written just before head.
  if (head_ % PER_SECTOR != 0) {
    LogRecord r;
    uint32_t sectorEnd = (head_ / PER_SECTOR + 1) * PER_SECTOR;
    bool restBlank = true;
    for (uint32_t s = head_; s < sectorEnd && restBlank; s++)
      restBlank = dev.read(s * REC, &r, REC) && blank(r);
    if (restBlank) {
      writable_ = sectorEnd - head_;
    } else {
      head_ = sectorEnd % slots_;  // a torn write after the newest record: leave that sector's tail
      writable_ = 0;
    }
  } else {
    writable_ = 0;
  }
  // Whole erased sectors after that, stopping before head's own sector.
  while (writable_ + head_ % PER_SECTOR + PER_SECTOR <= slots_) {
    uint32_t end = (head_ + writable_) % slots_;
    if (!sectorErased(end / PER_SECTOR)) break;
    writable_ += PER_SECTOR;
  }
  return true;
}

bool LogRing::sectorErased(uint32_t s) {
  LogRecord buf[CHUNK];
  for (uint32_t i = 0; i < PER_SECTOR; i += CHUNK) {
    if (!dev_->read((s * PER_SECTOR + i) * REC, buf, sizeof(buf))) return false;
    for (const LogRecord& r : buf)
      if (!blank(r)) return false;
  }
  return true;
}

bool LogRing::append(LogRecord& r) {
  if (!dev_ || writable_ == 0) return false;
  seal(r, nextSeq_);
  if (!dev_->write(head_ * REC, &r, REC)) {
    // The slot may now hold part of a record: skip it, it reads as torn.
    head_ = (head_ + 1) % slots_;
    writable_--;
    return false;
  }
  nextSeq_++;
  count_++;
  head_ = (head_ + 1) % slots_;
  writable_--;
  return true;
}

bool LogRing::maintain(uint32_t target) {
  if (!dev_ || writable_ >= target * PER_SECTOR) return false;
  // Never erase the sector that holds the records just before head.
  if (writable_ + head_ % PER_SECTOR + PER_SECTOR > slots_) return false;
  uint32_t s = ((head_ + writable_) % slots_) / PER_SECTOR;
  // The oldest records go: take them off the count first.
  LogRecord buf[CHUNK];
  for (uint32_t i = 0; i < PER_SECTOR; i += CHUNK) {
    if (!dev_->read((s * PER_SECTOR + i) * REC, buf, sizeof(buf))) return false;
    for (const LogRecord& r : buf)
      if (!blank(r) && intact(r) && count_ > 0) count_--;
  }
  if (!dev_->eraseSector(s * SECTOR)) return false;
  writable_ += PER_SECTOR;
  return true;
}

uint32_t LogRing::room() const { return writable_; }

bool LogRing::newest(uint32_t k, LogRecord& out) {
  if (!dev_) return false;
  uint32_t slot = head_;
  for (uint32_t step = 0; step < slots_; step++) {
    slot = (slot + slots_ - 1) % slots_;
    if (!dev_->read(slot * REC, &out, REC)) return false;
    if (blank(out) || !intact(out)) continue;
    if (k == 0) return true;
    k--;
  }
  return false;
}

}  // namespace rideon
