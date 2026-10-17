// Radio channel numbers and decoding. Must match docs/firmware/CHANNEL_MAP.md.
//
// AlfredoCRSF's getChannel() is 1-based: channel 1 is the first channel.
// The old sketches passed 0 for throttle, which read past the array.
#pragma once

#include <stdint.h>

namespace rideon {
namespace ch {

constexpr uint8_t STEERING = 1;   // right stick horizontal (aileron)
constexpr uint8_t THROTTLE = 2;   // right stick vertical (elevator), springs back to centre
constexpr uint8_t KILL = 5;       // AUX1, 2-position: high = run ("armed" in ELRS terms), anything else = KILL
constexpr uint8_t MODE = 6;       // AUX2, 3-position: low = Locked, middle = Kid, high = Parent
constexpr uint8_t SPEED_CAP = 7;  // AUX3, 3-position: Kid speed cap low / middle / high
constexpr uint8_t HORN = 8;       // AUX4, momentary

constexpr uint8_t COUNT = 8;  // channels the firmware reads

}  // namespace ch

struct ChannelLimits {
  uint16_t minValid;
  uint16_t maxValid;
  uint16_t low;
  uint16_t center;
  uint16_t high;
  uint16_t deadband;
  uint16_t switchLowBelow;
  uint16_t switchHighAbove;
};

inline bool channelValid(uint16_t us, const ChannelLimits& l) {
  return us >= l.minValid && us <= l.maxValid;
}

// Stick value in microseconds to -1..+1, with a centre deadband.
// Invalid values return 0 (stop); the caller raises FAULT_INPUT separately.
inline float stickFraction(uint16_t us, const ChannelLimits& l) {
  if (!channelValid(us, l)) return 0.0f;
  int d = (int)us - (int)l.center;
  if (d > -(int)l.deadband && d < (int)l.deadband) return 0.0f;
  float span = d > 0 ? (float)(l.high - l.center - l.deadband) : (float)(l.center - l.low - l.deadband);
  float v = (float)(d > 0 ? d - (int)l.deadband : d + (int)l.deadband) / span;
  if (v > 1.0f) v = 1.0f;
  if (v < -1.0f) v = -1.0f;
  return v;
}

// Switch position: 0 = low, 1 = middle, 2 = high, -1 = invalid.
inline int switchPosition(uint16_t us, const ChannelLimits& l) {
  if (!channelValid(us, l)) return -1;
  if (us < l.switchLowBelow) return 0;
  if (us > l.switchHighAbove) return 2;
  return 1;
}

}  // namespace rideon
