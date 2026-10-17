#include "RadioInput.h"

#include "pins.h"

namespace {
constexpr uint32_t CRSF_BAUD = 420000;
constexpr uint8_t FRAME_RC_CHANNELS = 0x16;

uint8_t crc8DvbS2(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
  }
  return crc;
}
}  // namespace

int RadioInput::TapStream::read() {
  int b = inner_.read();
  if (b >= 0) feed((uint8_t)b);
  return b;
}

// Frame: [address][length][type][payload...][crc]; length counts type..crc.
void RadioInput::TapStream::feed(uint8_t b) {
  if (pos_ == 0) {
    if (b != 0xC8 && b != 0xEE && b != 0xEA && b != 0xEC) return;  // not a sync byte
  } else if (pos_ == 1 && (b < 2 || b > 62)) {
    pos_ = 0;
    return;
  }
  buf_[pos_++] = b;
  if (pos_ < 2) return;
  uint8_t total = buf_[1] + 2;
  if (pos_ < total) return;
  uint8_t len = buf_[1];
  if (buf_[2] == FRAME_RC_CHANNELS && crc8DvbS2(&buf_[2], len - 1) == buf_[total - 1]) {
    frames++;
    lastFrameMs = millis();
  }
  pos_ = 0;
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
