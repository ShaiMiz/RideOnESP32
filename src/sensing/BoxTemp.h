// Box temperature from a DS18B20 on GPIO15 (1-Wire, 4.7k pull-up).
//
// The 1-Wire timing is done here with the pin in open-drain mode, so no
// library is needed. Nothing waits for the 750 ms conversion: update() takes
// one small step per call (start a conversion, then read the 9-byte
// scratchpad three bytes at a time), so no call holds the loop for more than
// about 2 ms. 1-Wire slaves wait as long as needed between bit slots, which
// is what makes splitting the read safe.
#pragma once

#include <stdint.h>

class BoxTemp {
 public:
  void begin();
  void update(uint32_t nowMs);

  bool hasReading() const { return hasReading_; }
  float celsius() const { return celsius_; }
  uint32_t lastGoodMs() const { return lastGoodMs_; }

 private:
  enum class Step : uint8_t { Idle, Converting, Reading };

  bool reset();
  void writeByte(uint8_t b);
  uint8_t readByte();

  Step step_ = Step::Idle;
  uint32_t stepMs_ = 0;
  int got_ = 0;
  uint8_t pad_[9] = {};
  bool hasReading_ = false;
  float celsius_ = 0.0f;
  uint32_t lastGoodMs_ = 0;
};
