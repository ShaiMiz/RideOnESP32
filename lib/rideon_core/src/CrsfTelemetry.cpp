#include "CrsfTelemetry.h"

#include <stdio.h>
#include <string.h>

#include "CrsfFrameDetector.h"

namespace rideon {

namespace {

uint32_t clampedTenths(float v, uint32_t max) {
  if (!(v > 0.0f)) return 0;  // also NaN
  float t = v * 10.0f + 0.5f;
  return t >= (float)max ? max : (uint32_t)t;
}

uint8_t finish(uint8_t* buf, uint8_t type, uint8_t payloadLen) {
  buf[0] = CRSF_ADDRESS_FC;
  buf[1] = (uint8_t)(payloadLen + 2);  // type + payload + crc
  buf[2] = type;
  buf[3 + payloadLen] = crsfCrc8(&buf[2], (uint8_t)(payloadLen + 1));
  return (uint8_t)(payloadLen + 4);
}

}  // namespace

uint8_t crsfBatteryFrame(uint8_t* buf, float volts, float amps, uint32_t usedMah, uint8_t remainingPct) {
  uint32_t dv = clampedTenths(volts, 0xFFFF);
  uint32_t da = clampedTenths(amps, 0xFFFF);
  if (usedMah > 0xFFFFFF) usedMah = 0xFFFFFF;
  uint8_t* p = &buf[3];  // big-endian, as CRSF sends every field
  p[0] = (uint8_t)(dv >> 8);
  p[1] = (uint8_t)dv;
  p[2] = (uint8_t)(da >> 8);
  p[3] = (uint8_t)da;
  p[4] = (uint8_t)(usedMah >> 16);
  p[5] = (uint8_t)(usedMah >> 8);
  p[6] = (uint8_t)usedMah;
  p[7] = remainingPct > 100 ? 100 : remainingPct;
  return finish(buf, CRSF_TYPE_BATTERY, 8);
}

uint8_t crsfFlightModeFrame(uint8_t* buf, const char* text) {
  size_t n = strnlen(text, CRSF_STATUS_MAX - 1);
  memcpy(&buf[3], text, n);
  buf[3 + n] = '\0';
  return finish(buf, CRSF_TYPE_FLIGHT_MODE, (uint8_t)(n + 1));
}

void statusText(char* out, size_t size, Mode mode, FaultMask latched) {
  if (latched == FAULT_NONE) {
    snprintf(out, size, "%s", modeName(mode));
    return;
  }
  FaultMask first = (FaultMask)(latched & (~latched + 1u));  // lowest bit, as faultName() picks
  snprintf(out, size, "%s %s%s", modeName(mode), faultName(first), latched != first ? "+" : "");
}

uint8_t TelemetryScheduler::update(const TelemetryInputs& in, uint8_t* buf) {
  if (started_) {
    uint32_t gap = in.nowMs - lastTickMs_;
    if (gap <= MAX_GAP_MS && in.driveAmps > 0.0f) usedMas_ += in.driveAmps * 1000.0f * (gap / 1000.0f);
  }
  started_ = true;
  lastTickMs_ = in.nowMs;
  pending_ = Pending::None;

  if (!in.linkUp) return 0;
  if (slotSent_ && (uint32_t)(in.nowMs - lastSlotMs_) < SLOT_MS) return 0;

  statusText(pendingStatus_, sizeof(pendingStatus_), in.mode, in.latched);
  bool statusDue = !statusSent_ || strcmp(pendingStatus_, lastStatus_) != 0 ||
                   (uint32_t)(in.nowMs - lastStatusMs_) >= STATUS_REPEAT_MS;
  pendingMs_ = in.nowMs;
  if (statusDue) {
    pending_ = Pending::Status;
    return crsfFlightModeFrame(buf, pendingStatus_);
  }
  pending_ = Pending::Battery;
  // Remaining charge needs the battery chemistry (#13); 0 until then.
  return crsfBatteryFrame(buf, in.batteryV, in.driveAmps, usedMah(), 0);
}

void TelemetryScheduler::sent() {
  if (pending_ == Pending::None) return;
  lastSlotMs_ = pendingMs_;
  slotSent_ = true;
  if (pending_ == Pending::Status) {
    memcpy(lastStatus_, pendingStatus_, sizeof(lastStatus_));
    lastStatusMs_ = pendingMs_;
    statusSent_ = true;
  }
  pending_ = Pending::None;
}

}  // namespace rideon
