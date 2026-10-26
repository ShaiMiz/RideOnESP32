#include "FaultTable.h"

namespace rideon {

int FaultTable::indexOf(uint8_t bit, uint8_t comp) const {
  for (int i = 0; i < count_; i++)
    if (rows_[i].faultBit == bit && rows_[i].comp == comp) return i;
  return -1;
}

const FaultRow* FaultTable::trip(const FaultReport& r, const FreezeFrame& frame) {
  uint8_t bit = faultBit(r.fault);
  uint8_t comp = (uint8_t)r.comp;
  int16_t value = scaled(r.value, 100.0f);
  bool lowIsWorse = r.sym == Symptom::UnderLimit;
  int i = indexOf(bit, comp);
  if (i < 0) {
    if (count_ >= CAPACITY) return nullptr;
    i = count_++;
    rows_[i] = {};
    rows_[i].faultBit = bit;
    rows_[i].comp = comp;
    rows_[i].worst = value;
    rows_[i].first = frame;
  } else if (lowIsWorse ? value < rows_[i].worst : value > rows_[i].worst) {
    rows_[i].worst = value;
  }
  FaultRow& row = rows_[i];
  row.sym = (uint8_t)r.sym;
  row.limit = scaled(r.limit, 100.0f);
  if (row.count < 0xFFFF) row.count++;
  row.status |= DTC_FAILING_NOW | DTC_FAILED_THIS_BOOT | DTC_CONFIRMED | DTC_FAILED_SINCE_CLEAR;
  row.last = frame;
  dirty_ = true;
  return &row;
}

void FaultTable::gone(FaultMask fault, Component comp) {
  int i = indexOf(faultBit(fault), (uint8_t)comp);
  if (i < 0 || !(rows_[i].status & DTC_FAILING_NOW)) return;
  rows_[i].status &= (uint8_t)~DTC_FAILING_NOW;
  dirty_ = true;
}

void FaultTable::newBoot() {
  for (int i = 0; i < count_; i++) rows_[i].status &= (uint8_t)~(DTC_FAILING_NOW | DTC_FAILED_THIS_BOOT);
}

void FaultTable::clear() {
  count_ = 0;
  dirty_ = true;
}

const FaultRow* FaultTable::find(FaultMask fault, Component comp) const {
  int i = indexOf(faultBit(fault), (uint8_t)comp);
  return i < 0 ? nullptr : &rows_[i];
}

bool FaultTable::takeDirty() {
  bool d = dirty_;
  dirty_ = false;
  return d;
}

}  // namespace rideon
