#include "Recorder.h"

namespace rideon {

LogRecord Recorder::record(Event e, Severity s, uint32_t nowMs) const {
  return makeRecord(e, s, boot_, nowMs, (uint8_t)mode_, latched_);
}

bool Recorder::counts(const FaultReport& r) const {
  if (r.sym == Symptom::Waiting) return false;
  if (r.fault == FAULT_LINK_LOST && !linkSeenOk_) return false;
  return true;
}

void Recorder::begin(uint16_t boot, ResetReason reason, uint32_t nowMs, RecordQueue& q) {
  boot_ = boot;
  LogRecord r = record(Event::Boot, Severity::Notice, nowMs);
  r.code = (uint8_t)reason;
  r.v[0] = (int16_t)boot;
  q.push(r);
  lastRecordMs_ = nowMs;
}

void Recorder::setupFailed(Component comp, uint32_t nowMs, RecordQueue& q, FaultTable& table) {
  latched_ |= FAULT_SETUP;
  LogRecord r = record(Event::SetupFail, Severity::Crit, nowMs);
  r.comp = (uint8_t)comp;
  q.push(r);
  FreezeFrame frame = {};
  frame.boot = boot_;
  frame.tMs = nowMs;
  frame.boxC = -128;
  if (!table.trip({FAULT_SETUP, comp, Symptom::NoAnswer, 0.0f, 0.0f}, frame)) {
    LogRecord full = record(Event::TableFull, Severity::Warn, nowMs);
    full.code = faultBit(FAULT_SETUP);
    full.comp = (uint8_t)comp;
    q.push(full);
  }
}

void Recorder::update(const RecorderInputs& in, RecordQueue& q, FaultTable& table) {
  const uint32_t now = in.nowMs;
  const int queuedBefore = q.size();

  uint32_t dropped = q.takeDropped();
  if (dropped) {
    LogRecord r = record(Event::Dropped, Severity::Warn, now);
    r.v[0] = dropped > 32767 ? 32767 : (int16_t)dropped;
    q.push(r);
  }

  if (in.mode != mode_) {
    LogRecord r = record(Event::ModeChange, Severity::Notice, now);
    r.v[0] = (int16_t)mode_;
    r.v[1] = (int16_t)in.mode;
    mode_ = in.mode;
    r.mode = (uint8_t)mode_;
    q.push(r);
  }

  if (in.link != link_) {
    link_ = in.link;
    if (link_ == LinkState::Ok) linkSeenOk_ = true;
    LogRecord r = record(Event::LinkChange, link_ == LinkState::Ok ? Severity::Notice : Severity::Warn, now);
    r.code = (uint8_t)link_;
    r.v[0] = in.linkQuality;
    q.push(r);
  }

  if (in.battery != battery_) {
    battery_ = in.battery;
    Severity s = battery_ == BatteryState::Ok    ? Severity::Notice
                 : battery_ == BatteryState::Low ? Severity::Warn
                                                 : Severity::Err;
    LogRecord r = record(Event::Battery, s, now);
    r.code = (uint8_t)battery_;
    r.v[0] = scaled(in.batteryVolts, 100.0f);
    q.push(r);
  }

  // Faults that went away on a part since last loop.
  FaultMask latched = in.faults ? in.faults->latched() : (FaultMask)FAULT_NONE;
  FaultReports next;
  uint32_t nextSince[FaultReports::CAPACITY] = {};
  for (int i = 0; i < prev_.size(); i++) {
    const FaultReport& p = prev_.at(i);
    if (in.reports && in.reports->find(p.fault, p.comp) >= 0) continue;
    LogRecord r = record(Event::FaultGone, Severity::Notice, now);
    r.code = faultBit(p.fault);
    r.comp = (uint8_t)p.comp;
    r.v[0] = scaled((now - since_[i]) / 1000.0f, 10.0f);
    q.push(r);
    table.gone(p.fault, p.comp);
  }

  // A re-arm cleared latched faults. Only the ModeMachine clears them, so
  // any bit that left the latch was cleared by a re-arm.
  FaultMask cleared = latched_ & (FaultMask)~latched;
  latched_ = latched;
  if (cleared != FAULT_NONE) {
    LogRecord r = record(Event::Rearm, Severity::Notice, now);
    r.v[0] = (int16_t)cleared;
    q.push(r);
  }

  // New trips, and the ones still running.
  FreezeFrame frame = in.frame;
  frame.boot = boot_;
  frame.tMs = now;
  frame.mode = (uint8_t)mode_;
  for (int i = 0; in.reports && i < in.reports->size(); i++) {
    const FaultReport& rep = in.reports->at(i);
    if (!counts(rep)) continue;
    int was = prev_.find(rep.fault, rep.comp);
    next.add(rep.fault, rep.comp, rep.sym, rep.value, rep.limit);
    int at = next.find(rep.fault, rep.comp);
    if (at < 0) continue;  // more parts failing than the list holds; the mask still has them
    if (was >= 0) {
      nextSince[at] = since_[was];
      continue;
    }
    nextSince[at] = now;
    LogRecord r = record(Event::Fault, (rep.fault & HARD_FAULTS) ? Severity::Crit : Severity::Err, now);
    r.code = faultBit(rep.fault);
    r.comp = (uint8_t)rep.comp;
    r.sym = (uint8_t)rep.sym;
    r.v[0] = scaled(rep.value, 100.0f);
    r.v[1] = scaled(rep.limit, 100.0f);
    q.push(r);
    if (!table.trip(rep, frame)) {
      LogRecord full = record(Event::TableFull, Severity::Warn, now);
      full.code = r.code;
      full.comp = r.comp;
      q.push(full);
    }
  }
  prev_ = next;
  for (int i = 0; i < FaultReports::CAPACITY; i++) since_[i] = nextSince[i];

  if (q.size() != queuedBefore) {
    lastRecordMs_ = now;
  } else if (now - lastRecordMs_ >= SNAPSHOT_MS) {
    LogRecord r = record(Event::Snapshot, Severity::Info, now);
    r.v[0] = scaled(in.batteryVolts, 100.0f);
    r.v[1] = scaled(in.driveAmps, 100.0f);
    r.v[2] = scaled(in.output, 100.0f);
    r.v[3] = in.linkQuality;
    q.push(r);
    lastRecordMs_ = now;
  }
}

}  // namespace rideon
