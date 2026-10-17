// Rules every pin map must pass. pins.h runs them with static_assert, so a
// bad pin map stops every build (car, bench and native tests). The native
// test test/test_pins runs them again and names the offending pin.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "board_esp32_devkit30.h"

namespace pinrules {

enum class Use : uint8_t {
  Pwm,       // motor PWM output
  KillLine,  // safety output, external pull-down, LOW = off
  UartRx,
  UartTx,
  I2c,       // open drain, external pull-up
  OneWire,   // open drain, external pull-up
  Analog,    // ADC input
  Pulse,     // counted edges or interrupts
};

struct PinUse {
  uint8_t gpio;
  const char* name;
  Use use;
};

constexpr bool isOutput(Use u) { return u != Use::UartRx && u != Use::Analog && u != Use::Pulse; }

// Each rule returns true when pin p (index i of map m) is fine.
// Rules that need the whole map take it too.

constexpr bool onBoard(const PinUse& p) {
  return board::onHeader(p.gpio) && !board::flash(p.gpio) && !board::usbSerial(p.gpio);
}

constexpr bool canDrive(const PinUse& p) { return !isOutput(p.use) || !board::inputOnly(p.gpio); }

constexpr bool analogOnAdc1(const PinUse& p) { return p.use != Use::Analog || board::adc1(p.gpio); }

constexpr bool pulseNotOnGlitchPin(const PinUse& p) { return p.use != Use::Pulse || !board::adcGlitch(p.gpio); }

// A kill line must sit LOW from power-on until our code says otherwise.
constexpr bool killLineQuietAtBoot(const PinUse& p) {
  return p.use != Use::KillLine ||
         (!board::activeDuringBoot(p.gpio) && (!board::strapping(p.gpio) || board::mustBeLowAtBoot(p.gpio)));
}

// Strapping pins only carry signals whose idle level matches the strap:
// kill lines (LOW) on 2/12, UART TX or 1-Wire (HIGH) on 5/15.
constexpr bool strappingUseMatches(const PinUse& p) {
  return !board::strapping(p.gpio) ||
         (board::mustBeLowAtBoot(p.gpio) && p.use == Use::KillLine) ||
         (board::highAtBoot(p.gpio) && (p.use == Use::UartTx || p.use == Use::OneWire));
}

using Rule = bool (*)(const PinUse&);

// Index of the first entry that breaks the rule, or -1.
constexpr int firstBreaking(const PinUse* m, size_t n, Rule rule) {
  for (size_t i = 0; i < n; i++)
    if (!rule(m[i])) return (int)i;
  return -1;
}

// Index of the first entry whose GPIO appeared earlier in the map, or -1.
constexpr int firstDuplicate(const PinUse* m, size_t n) {
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < i; j++)
      if (m[i].gpio == m[j].gpio) return (int)i;
  return -1;
}

constexpr int countUse(const PinUse* m, size_t n, Use u) {
  int c = 0;
  for (size_t i = 0; i < n; i++)
    if (m[i].use == u) c++;
  return c;
}

// Index of the first repeated byte in a list (I2C addresses, PCF8574 bits), or -1.
constexpr int firstRepeat(const uint8_t* v, size_t n) {
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < i; j++)
      if (v[i] == v[j]) return (int)i;
  return -1;
}

constexpr bool allValid(const PinUse* m, size_t n) {
  return firstDuplicate(m, n) < 0 && firstBreaking(m, n, onBoard) < 0 && firstBreaking(m, n, canDrive) < 0 &&
         firstBreaking(m, n, analogOnAdc1) < 0 && firstBreaking(m, n, pulseNotOnGlitchPin) < 0 &&
         firstBreaking(m, n, killLineQuietAtBoot) < 0 && firstBreaking(m, n, strappingUseMatches) < 0 &&
         countUse(m, n, Use::Pwm) <= board::PWM_CHANNELS;
}

}  // namespace pinrules
