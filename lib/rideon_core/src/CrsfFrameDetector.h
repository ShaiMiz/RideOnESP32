// Spots CRSF RC-channels frames in the receiver's byte stream and timestamps
// them, so LinkMonitor can judge the link by packet age.
//
// It resyncs exactly like AlfredoCRSF's parser: a bad length or CRC drops one
// byte and looks again. A detector that resynced differently could fall out
// of step with the library and miss frames the library accepts, which would
// read as link loss.
#pragma once

#include <stdint.h>

namespace rideon {

class CrsfFrameDetector {
 public:
  static constexpr uint8_t MAX_LEN = 64;              // CRSF_MAX_PACKET_LEN
  static constexpr uint8_t TYPE_RC_CHANNELS = 0x16;
  static constexpr uint32_t IDLE_FLUSH_MS = 100;      // AlfredoCRSF CRSF_PACKET_TIMEOUT_MS

  // Feed one received byte. Returns true if it completed a valid RC-channels frame.
  bool feed(uint8_t b, uint32_t nowMs);

  uint32_t frames() const { return frames_; }
  uint32_t lastFrameMs() const { return lastFrameMs_; }

 private:
  void shift(uint8_t n);

  uint8_t buf_[MAX_LEN + 3] = {};
  uint8_t pos_ = 0;
  uint32_t lastByteMs_ = 0;
  uint32_t frames_ = 0;
  uint32_t lastFrameMs_ = 0;
};

uint8_t crsfCrc8(const uint8_t* data, uint8_t len);

}  // namespace rideon
