#include "CrashRing.h"

#include <stddef.h>
#include <string.h>

#include "EventLog.h"
#include "ModeMachine.h"

namespace rideon {

namespace {
constexpr uint32_t MAGIC = 0x52494445;  // "RIDE"
}

uint16_t CrashRing::checksum() const { return crc16(reinterpret_cast<const uint8_t*>(&body_), sizeof(body_)); }

void CrashRing::reset(uint16_t boot) {
  memset(&body_, 0, sizeof(body_));
  body_.boot = boot;
  magic_ = MAGIC;
  crc_ = checksum();
}

void CrashRing::push(const FreezeFrame& f) {
  body_.samples[body_.head] = f;
  body_.head = (uint8_t)((body_.head + 1) % SAMPLES);
  if (body_.count < SAMPLES) body_.count++;
  crc_ = checksum();
}

bool CrashRing::intact() const {
  return magic_ == MAGIC && body_.count <= SAMPLES && body_.head < SAMPLES && crc_ == checksum();
}

const FreezeFrame& CrashRing::at(int i) const {
  int oldest = (body_.head + SAMPLES - body_.count) % SAMPLES;
  return body_.samples[(oldest + i) % SAMPLES];
}

void freeze(const CrashRing& ring, uint8_t cause, uint8_t code, SavedCrash& out) {
  memset(&out, 0, sizeof(out));
  out.format = FaultTable::FORMAT;
  out.cause = cause;
  out.code = code;
  out.boot = ring.boot();
  out.count = (uint8_t)ring.size();
  for (int i = 0; i < ring.size(); i++) out.samples[i] = ring.at(i);
}

bool CrashRecorder::drove(const CrashRing& ring) {
  for (int i = 0; i < ring.size(); i++) {
    Mode m = (Mode)ring.at(i).mode;
    if (m == Mode::Kid || m == Mode::Parent) return true;
  }
  return false;
}

bool CrashRecorder::begin(CrashRing& ring, uint16_t boot, ResetReason reason, SavedCrash& out) {
  bool unexpected = reason == ResetReason::Panic || reason == ResetReason::Watchdog || reason == ResetReason::Brownout;
  bool keep = unexpected && ring.intact() && drove(ring);
  if (keep) freeze(ring, CRASH_RESET, (uint8_t)reason, out);
  ring.reset(boot);
  sampled_ = false;
  hardBefore_ = FAULT_NONE;
  return keep;
}

bool CrashRecorder::update(CrashRing& ring, const FreezeFrame& frame, const FaultReports& reports, SavedCrash& out) {
  FaultMask hard = FAULT_NONE;
  for (int i = 0; i < reports.size(); i++) {
    const FaultReport& r = reports.at(i);
    if ((r.fault & HARD_FAULTS) && r.fault != FAULT_KILL_SWITCH && r.sym != Symptom::Waiting) hard |= r.fault;
  }
  FaultMask fresh = (FaultMask)(hard & ~hardBefore_);
  hardBefore_ = hard;

  bool due = !sampled_ || (uint32_t)(frame.tMs - lastSampleMs_) >= CrashRing::PERIOD_MS;
  if (due || fresh) {
    // The moment of the trip is always sampled, so it is the last frame.
    ring.push(frame);
    sampled_ = true;
    lastSampleMs_ = frame.tMs;
  }
  if (!fresh || !drove(ring)) return false;
  freeze(ring, CRASH_FAULT, faultBit(fresh), out);
  return true;
}

}  // namespace rideon
