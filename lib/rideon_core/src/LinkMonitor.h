// Decides whether the radio link is alive.
//
// The link is judged only by the receiver's link flag, the age of the last
// RC packet and the link quality. Never by channel values: AlfredoCRSF keeps
// the last values after the link drops, so a frozen throttle looks valid.
#pragma once

#include <stdint.h>

namespace rideon {

enum class LinkState : uint8_t { Lost, Degraded, Ok };

struct LinkParams {
  uint32_t timeoutMs;
  uint8_t lqDegradedBelow;
  uint8_t lqLostBelow;
};

struct LinkInputs {
  bool linkUp;            // crsf.isLinkUp()
  bool anyPacket;         // at least one RC packet since boot
  uint32_t lastPacketMs;  // millis() of the last RC packet
  uint8_t linkQuality;    // uplink link quality, 0-100 %
  uint32_t nowMs;
};

LinkState evaluateLink(const LinkInputs& in, const LinkParams& p);

const char* linkName(LinkState s);

}  // namespace rideon
