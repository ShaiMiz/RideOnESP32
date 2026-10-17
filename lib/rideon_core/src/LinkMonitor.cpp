#include "LinkMonitor.h"

namespace rideon {

LinkState evaluateLink(const LinkInputs& in, const LinkParams& p) {
  if (!in.linkUp || !in.anyPacket) return LinkState::Lost;
  uint32_t age = in.nowMs - in.lastPacketMs;  // unsigned: safe across millis() wrap
  if (age > p.timeoutMs) return LinkState::Lost;
  if (in.linkQuality < p.lqLostBelow) return LinkState::Lost;
  if (in.linkQuality < p.lqDegradedBelow) return LinkState::Degraded;
  return LinkState::Ok;
}

const char* linkName(LinkState s) {
  switch (s) {
    case LinkState::Lost: return "LOST";
    case LinkState::Degraded: return "WEAK";
    case LinkState::Ok: return "OK";
  }
  return "?";
}

}  // namespace rideon
