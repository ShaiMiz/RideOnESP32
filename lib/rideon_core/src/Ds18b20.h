// DS18B20 scratchpad decoding, kept free of 1-Wire timing so it can be unit
// tested. BoxTemp does the bus traffic.
#pragma once

#include <stdint.h>

namespace rideon {
namespace ds18b20 {

constexpr uint8_t SKIP_ROM = 0xCC;
constexpr uint8_t CONVERT_T = 0x44;
constexpr uint8_t READ_SCRATCHPAD = 0xBE;
constexpr int SCRATCHPAD_BYTES = 9;

// Dallas/Maxim CRC-8 (polynomial x^8 + x^5 + x^4 + 1, reflected 0x8C).
inline uint8_t crc8(const uint8_t* data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    uint8_t b = data[i];
    for (int bit = 0; bit < 8; bit++) {
      uint8_t mix = (crc ^ b) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      b >>= 1;
    }
  }
  return crc;
}

// Temperature from a 9-byte scratchpad. False when the CRC fails, when no
// sensor answered (the pull-up reads all 0xFF), or on 85.0 C, the power-on
// value a sensor shows when it reset instead of converting.
inline bool decodeCelsius(const uint8_t* pad, float& celsius) {
  bool allOnes = true;
  for (int i = 0; i < SCRATCHPAD_BYTES; i++) allOnes &= pad[i] == 0xFF;
  if (allOnes) return false;
  if (crc8(pad, SCRATCHPAD_BYTES - 1) != pad[SCRATCHPAD_BYTES - 1]) return false;
  int16_t raw = (int16_t)((pad[1] << 8) | pad[0]);
  if (raw == 0x0550) return false;
  celsius = raw / 16.0f;
  return true;
}

}  // namespace ds18b20
}  // namespace rideon
