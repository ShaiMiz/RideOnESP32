#include "BoxTemp.h"

#include <Arduino.h>

#include "Ds18b20.h"
#include "config.h"
#include "pins.h"

using namespace rideon;

namespace {

constexpr uint32_t CONVERSION_MS = 800;  // 750 ms at 12 bits, plus margin
constexpr int BYTES_PER_CALL = 3;

portMUX_TYPE oneWireMux = portMUX_INITIALIZER_UNLOCKED;

inline void busLow() { digitalWrite(pins::ONEWIRE_TEMP, LOW); }
inline void busRelease() { digitalWrite(pins::ONEWIRE_TEMP, HIGH); }  // open drain: the pull-up takes it HIGH

}  // namespace

void BoxTemp::begin() {
  busRelease();
  pinMode(pins::ONEWIRE_TEMP, OUTPUT_OPEN_DRAIN);
  busRelease();
  step_ = Step::Idle;
  stepMs_ = millis() - cfg::BOX_TEMP_PERIOD_MS;  // first conversion on the first update
}

// Reset pulse, then true if a sensor answers with a presence pulse.
bool BoxTemp::reset() {
  busLow();
  delayMicroseconds(480);
  portENTER_CRITICAL(&oneWireMux);
  busRelease();
  delayMicroseconds(70);
  bool present = digitalRead(pins::ONEWIRE_TEMP) == LOW;
  portEXIT_CRITICAL(&oneWireMux);
  delayMicroseconds(410);
  return present;
}

void BoxTemp::writeByte(uint8_t b) {
  for (int i = 0; i < 8; i++, b >>= 1) {
    portENTER_CRITICAL(&oneWireMux);
    busLow();
    if (b & 1) {
      delayMicroseconds(6);
      busRelease();
      delayMicroseconds(64);
    } else {
      delayMicroseconds(60);
      busRelease();
      delayMicroseconds(10);
    }
    portEXIT_CRITICAL(&oneWireMux);
  }
}

uint8_t BoxTemp::readByte() {
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    portENTER_CRITICAL(&oneWireMux);
    busLow();
    delayMicroseconds(3);
    busRelease();
    delayMicroseconds(10);
    bool one = digitalRead(pins::ONEWIRE_TEMP) == HIGH;
    portEXIT_CRITICAL(&oneWireMux);
    delayMicroseconds(53);
    if (one) b |= (uint8_t)(1u << i);
  }
  return b;
}

void BoxTemp::update(uint32_t nowMs) {
  switch (step_) {
    case Step::Idle:
      if ((uint32_t)(nowMs - stepMs_) < cfg::BOX_TEMP_PERIOD_MS) return;
      stepMs_ = nowMs;
      if (!reset()) return;  // no sensor: try again next period; the reading goes stale
      writeByte(ds18b20::SKIP_ROM);
      writeByte(ds18b20::CONVERT_T);
      step_ = Step::Converting;
      return;

    case Step::Converting:
      if ((uint32_t)(nowMs - stepMs_) < CONVERSION_MS) return;
      if (!reset()) {
        step_ = Step::Idle;
        return;
      }
      writeByte(ds18b20::SKIP_ROM);
      writeByte(ds18b20::READ_SCRATCHPAD);
      got_ = 0;
      step_ = Step::Reading;
      return;

    case Step::Reading:
      for (int i = 0; i < BYTES_PER_CALL && got_ < ds18b20::SCRATCHPAD_BYTES; i++) pad_[got_++] = readByte();
      if (got_ < ds18b20::SCRATCHPAD_BYTES) return;
      float c;
      if (ds18b20::decodeCelsius(pad_, c)) {
        celsius_ = c;
        hasReading_ = true;
        lastGoodMs_ = nowMs;
      }
      step_ = Step::Idle;  // stepMs_ still holds the conversion start, so readings come once a period
      return;
  }
}
