// Which part a fault came from, and by how much.
//
// The fault mask (Faults.h) says what is wrong; a report says where:
// FAULT_OVERCURRENT on the front-left wheel at 23.4 A against a 20 A limit.
// The safety checks file one report per fault and part each loop, beside
// the mask. The mask still decides how the car stops; the reports only feed
// the log and the fault table.
#pragma once

#include <stdint.h>

#include "EventLog.h"
#include "Faults.h"

namespace rideon {

struct FaultReport {
  FaultMask fault;  // one bit
  Component comp;
  Symptom sym;
  float value;  // what was measured, in the unit of the check (A, V, C); 0 if none
  float limit;  // the limit it crossed; 0 if none
};

class FaultReports {
 public:
  static constexpr int CAPACITY = 24;

  void clear() {
    count_ = 0;
    mask_ = FAULT_NONE;
  }

  // One report per fault and part: a second one for the same pair is ignored.
  // Returns the fault, so a check can write `f |= reports.add(...)`.
  FaultMask add(FaultMask fault, Component comp, Symptom sym, float value = 0.0f, float limit = 0.0f) {
    mask_ |= fault;
    if (find(fault, comp) >= 0) return fault;
    if (count_ < CAPACITY) items_[count_++] = {fault, comp, sym, value, limit};
    return fault;
  }

  int find(FaultMask fault, Component comp) const {
    for (int i = 0; i < count_; i++)
      if (items_[i].fault == fault && items_[i].comp == comp) return i;
    return -1;
  }

  int size() const { return count_; }
  const FaultReport& at(int i) const { return items_[i]; }
  FaultMask mask() const { return mask_; }

 private:
  FaultReport items_[CAPACITY] = {};
  int count_ = 0;
  FaultMask mask_ = FAULT_NONE;
};

}  // namespace rideon
