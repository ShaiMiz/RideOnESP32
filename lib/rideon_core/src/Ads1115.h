// ADS1115 register values and conversions, kept free of I2C so they can be
// unit tested. PowerMonitor does the bus traffic and checks every transfer.
#pragma once

#include <stdint.h>

namespace rideon {
namespace ads1115 {

constexpr uint8_t REG_CONVERSION = 0x00;
constexpr uint8_t REG_CONFIG = 0x01;

// Config for one single-shot conversion of single-ended input 0-3:
// start (OS), mux AINx vs GND, +/-4.096 V (covers the 3.3 V clamp),
// single-shot mode, 860 samples/s, comparator off.
inline uint16_t singleShotConfig(uint8_t input) {
  return (uint16_t)(0x8000u | ((0x4u + (input & 0x3u)) << 12) | 0x0200u | 0x0100u | 0x00E0u | 0x0003u);
}

// The OS bit reads 1 once the conversion has finished.
inline bool conversionDone(uint16_t config) { return (config & 0x8000u) != 0; }

// One count is 125 uV at the +/-4.096 V range.
inline float rawToVolts(int16_t raw) { return (float)raw * (4.096f / 32768.0f); }

}  // namespace ads1115

// True when a reading taken at lastMs is older than maxAgeMs (millis() wrap safe).
inline bool readingStale(uint32_t lastMs, uint32_t nowMs, uint32_t maxAgeMs) {
  return (uint32_t)(nowMs - lastMs) > maxAgeMs;
}

}  // namespace rideon
