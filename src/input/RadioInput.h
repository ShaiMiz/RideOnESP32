// ExpressLRS receiver input over CRSF (UART2, 420000 baud) via AlfredoCRSF.
//
// AlfredoCRSF does not expose when the last RC packet arrived, so the serial
// port is wrapped in a small tap that spots valid RC-channels frames and
// timestamps them. LinkMonitor then judges the link on that packet age, the
// library's link flag and the link quality, never on channel values.
#pragma once

#include <AlfredoCRSF.h>
#include <Arduino.h>

#include "LinkMonitor.h"

class RadioInput {
 public:
  bool begin();
  void update();

  // Channel value in microseconds. Channels are 1-based (see ChannelMap.h).
  uint16_t channel(uint8_t ch) const;

  bool linkUp() const { return crsf_.isLinkUp(); }
  bool anyPacket() const { return tap_.frames > 0; }
  uint32_t lastPacketMs() const { return tap_.lastFrameMs; }
  uint8_t linkQuality() const;
  rideon::LinkInputs linkInputs(uint32_t nowMs) const;

  AlfredoCRSF& crsf() { return crsf_; }

 private:
  // Passes bytes from the UART to AlfredoCRSF and records RC-channels frames.
  class TapStream : public Stream {
   public:
    explicit TapStream(Stream& inner) : inner_(inner) {}
    int available() override { return inner_.available(); }
    int peek() override { return inner_.peek(); }
    int read() override;
    size_t write(uint8_t b) override { return inner_.write(b); }
    size_t write(const uint8_t* buf, size_t len) override { return inner_.write(buf, len); }
    void flush() override { inner_.flush(); }

    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;

   private:
    void feed(uint8_t b);
    Stream& inner_;
    uint8_t buf_[64];
    uint8_t pos_ = 0;
  };

  TapStream tap_{Serial2};
  AlfredoCRSF crsf_;
};
