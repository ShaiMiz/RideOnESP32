#include "CrsfFrameDetector.h"

namespace rideon {

uint8_t crsfCrc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
  }
  return crc;
}

void CrsfFrameDetector::shift(uint8_t n) {
  if (n >= pos_) {
    pos_ = 0;
    return;
  }
  for (uint8_t i = 0; i < pos_ - n; i++) buf_[i] = buf_[i + n];
  pos_ -= n;
}

// Frame: [sync][length][type][payload...][crc]; length counts type..crc.
bool CrsfFrameDetector::feed(uint8_t b, uint32_t nowMs) {
  if (pos_ > 0 && (uint32_t)(nowMs - lastByteMs_) > IDLE_FLUSH_MS) pos_ = 0;
  lastByteMs_ = nowMs;
  buf_[pos_++] = b;

  bool rc = false;
  while (pos_ > 1) {
    uint8_t len = buf_[1];
    if (len < 3 || len > MAX_LEN) {
      shift(1);
      continue;
    }
    if (pos_ < len + 2) break;
    if (crsfCrc8(&buf_[2], len - 1) == buf_[len + 1]) {
      if (buf_[2] == TYPE_RC_CHANNELS) {
        rc = true;
        frames_++;
        lastFrameMs_ = nowMs;
      }
      shift(len + 2);
    } else {
      shift(1);
    }
  }
  if (pos_ >= sizeof(buf_)) pos_ = 0;
  return rc;
}

}  // namespace rideon
