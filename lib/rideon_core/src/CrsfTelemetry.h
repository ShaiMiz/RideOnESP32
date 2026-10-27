// Telemetry back to the radio over CRSF: battery voltage, drive current and
// charge used (CRSF battery sensor, 0x08), and a short status text with the
// mode and the latched fault (CRSF flight mode, 0x21). EdgeTX shows them as
// the RxBt, Curr, Capa and FM sensors (radio/MODEL_SETUP.md).
//
// The scheduler picks at most one frame per tick and only ever builds bytes;
// src/telemetry/CrsfTelemetry writes them when the UART has room, so
// telemetry never delays the loop. It cannot change anything the car does.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Faults.h"
#include "ModeMachine.h"

namespace rideon {

constexpr uint8_t CRSF_ADDRESS_FC = 0xC8;  // frames from the flight controller (us) to the receiver
constexpr uint8_t CRSF_TYPE_BATTERY = 0x08;
constexpr uint8_t CRSF_TYPE_FLIGHT_MODE = 0x21;
constexpr uint8_t CRSF_STATUS_MAX = 16;    // text, with its terminating zero
constexpr uint8_t CRSF_TELEMETRY_MAX = 4 + CRSF_STATUS_MAX;  // largest frame we build

// [address][length][type][payload][crc]. Return the frame length.
uint8_t crsfBatteryFrame(uint8_t* buf, float volts, float amps, uint32_t usedMah, uint8_t remainingPct);
uint8_t crsfFlightModeFrame(uint8_t* buf, const char* text);

// "PARENT", "KID", "LOCKED", "KILLED", followed by the first latched fault
// and "+" when there are more: "LOCKED ESTOP", "KILLED LINK+".
void statusText(char* out, size_t size, Mode mode, FaultMask latched);

struct TelemetryInputs {
  uint32_t nowMs;
  bool linkUp;       // the receiver only forwards telemetry with a link
  Mode mode;
  FaultMask latched;
  float batteryV;
  float driveAmps;
};

class TelemetryScheduler {
 public:
  static constexpr uint32_t SLOT_MS = 200;           // one frame per slot, at most 5 frames/s
  static constexpr uint32_t STATUS_REPEAT_MS = 1000;  // and sooner when the text changes
  static constexpr uint32_t MAX_GAP_MS = 1000;        // longer gaps between ticks are not integrated

  // Call every tick. Builds the frame due now into buf (at least
  // CRSF_TELEMETRY_MAX bytes) and returns its length, or 0. Call sent() once
  // the frame is written; a frame that is not sent is built again next tick.
  uint8_t update(const TelemetryInputs& in, uint8_t* buf);
  void sent();

  uint32_t usedMah() const { return (uint32_t)(usedMas_ / 3600.0f); }

 private:
  enum class Pending : uint8_t { None, Battery, Status };

  float usedMas_ = 0.0f;  // milliamp-seconds
  bool started_ = false;
  uint32_t lastTickMs_ = 0;
  uint32_t lastSlotMs_ = 0;
  uint32_t lastStatusMs_ = 0;
  bool slotSent_ = false;
  bool statusSent_ = false;
  char lastStatus_[CRSF_STATUS_MAX] = {};
  char pendingStatus_[CRSF_STATUS_MAX] = {};
  Pending pending_ = Pending::None;
  uint32_t pendingMs_ = 0;
};

}  // namespace rideon
