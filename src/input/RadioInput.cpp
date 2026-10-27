#include "RadioInput.h"

#include "pins.h"

namespace {
constexpr uint32_t CRSF_BAUD = 420000;
}  // namespace

int RadioInput::TapStream::read() {
  int b = inner_.read();
  if (b >= 0) frames.feed((uint8_t)b, millis());
  return b;
}

bool RadioInput::begin() {
  Serial2.begin(CRSF_BAUD, SERIAL_8N1, pins::CRSF_RX, pins::CRSF_TX);
  crsf_.begin(tap_);
  return true;
}

void RadioInput::update() { crsf_.update(); }

uint16_t RadioInput::channel(uint8_t ch) const {
  if (ch < 1 || ch > 16) return 0;  // invalid; decodes as out of range
  int v = crsf_.getChannel(ch);
  return v < 0 ? 0 : (uint16_t)v;
}

uint8_t RadioInput::linkQuality() const { return crsf_.getLinkStatistics()->uplink_Link_quality; }

rideon::LinkInputs RadioInput::linkInputs(uint32_t nowMs) const {
  return {linkUp(), anyPacket(), lastPacketMs(), linkQuality(), nowMs};
}

bool RadioInput::sendFrame(const uint8_t* buf, uint8_t len) {
  if (Serial2.availableForWrite() < (int)len) return false;
  return Serial2.write(buf, len) == len;
}
