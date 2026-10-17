// Build phase 2 bench firmware: radio on the desk.
//
// Only the receiver is connected. Every kill line is held LOW, so nothing
// can move. Prints link state, packet age, link quality and channels 1-8
// ten times a second.
//
// Gate (docs/COMMISSIONING.md, phase 2):
//   - turning the radio off shows link=LOST within 200 ms
//   - every stick and switch moves the channel CHANNEL_MAP.md says it does
#include <Arduino.h>

#include "ChannelMap.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"
#include "config.h"
#include "input/RadioInput.h"
#include "safety/KillLines.h"

using namespace rideon;

namespace {
KillLines lines;
RadioInput radio;
const LinkParams LINK = {cfg::LINK_TIMEOUT_MS, cfg::LQ_DEGRADED_BELOW, cfg::LQ_LOST_BELOW};
const ChannelLimits CHANNELS = {cfg::CH_MIN_VALID, cfg::CH_MAX_VALID,      cfg::CH_LOW,          cfg::CH_CENTER,
                                cfg::CH_HIGH,      cfg::STICK_DEADBAND_US, cfg::SWITCH_LOW_BELOW, cfg::SWITCH_HIGH_ABOVE};
uint32_t lastPrintMs = 0;
LinkState lastLink = LinkState::Lost;
uint32_t lostSinceMs = 0;

const char* modeRequestName(ModeRequest r) {
  switch (r) {
    case ModeRequest::Locked: return "LOCKED";
    case ModeRequest::Kid: return "KID";
    case ModeRequest::Parent: return "PARENT";
    default: return "INVALID";
  }
}
}  // namespace

void setup() {
  lines.begin();
  Serial.begin(115200);
  radio.begin();
  Serial.println("radio_check: kill lines LOW, waiting for the receiver");
}

void loop() {
  radio.update();
  uint32_t now = millis();
  LinkState link = evaluateLink(radio.linkInputs(now), LINK);

  // Report the moment the link changes, with the packet age that caused it.
  if (link != lastLink) {
    if (link == LinkState::Lost) lostSinceMs = now;
    Serial.printf(">>> link %s -> %s (last packet %lu ms ago)\n", linkName(lastLink), linkName(link),
                  (unsigned long)(now - radio.lastPacketMs()));
    lastLink = link;
  }

  if (now - lastPrintMs < 100) return;
  lastPrintMs = now;
  Serial.printf("link=%-4s up=%d age=%4lums lq=%3u |", linkName(link), radio.linkUp(),
                radio.anyPacket() ? (unsigned long)(now - radio.lastPacketMs()) : 0UL, radio.linkQuality());
  for (uint8_t c = 1; c <= ch::COUNT; c++) Serial.printf(" ch%u=%4u", c, radio.channel(c));
  Serial.printf(" | thr=%+.2f str=%+.2f run=%d mode=%s\n", stickFraction(radio.channel(ch::THROTTLE), CHANNELS),
                stickFraction(radio.channel(ch::STEERING), CHANNELS),
                switchPosition(radio.channel(ch::KILL), CHANNELS) == 2,
                modeRequestName(modeRequestFromSwitch(switchPosition(radio.channel(ch::MODE), CHANNELS))));
  (void)lostSinceMs;
}
