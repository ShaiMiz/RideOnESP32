#include "BoxTempCheck.h"

namespace rideon {

bool BoxTempCheck::update(bool hasReading, float celsius, uint32_t lastGoodMs, uint32_t nowMs,
                          const BoxTempParams& p) {
  bool fresh = hasReading && (uint32_t)(nowMs - lastGoodMs) <= p.staleMs;
  lost_ = !fresh;

  if (fresh) {
    if (above_.update(celsius > p.limitC, nowMs, p.holdMs)) overTemp_ = true;
    if (overTemp_ && celsius < p.recoverC) overTemp_ = false;
  } else {
    above_.update(false, nowMs, p.holdMs);  // an old reading says nothing new; keep overTemp_ as it was
  }

  hot_ = overTemp_ || (p.required && lost_);
  return hot_;
}

}  // namespace rideon
