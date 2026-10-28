#include "EventLog.h"

#include <stdio.h>
#include <string.h>

#include "BatteryCheck.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

namespace rideon {

namespace {

const char* const CATEGORY_NAMES[] = {"SYS", "SAFETY", "MODE", "LINK", "POWER", "DRIVE", "STEER", "SENSE", "CFG"};
const char* const COMPONENT_NAMES[] = {"-",     "FL",    "FR",  "RL",  "RR",  "STEER", "WHEELS", "BUS", "K1",
                                       "BATT",  "RADIO", "ESTOP", "PCF", "PCA", "ADS",   "BOX",    "IMU"};
const char* const SYMPTOM_NAMES[] = {"-",       "OVER",    "UNDER",  "STALE",  "STUCK_HIGH", "RANGE",
                                     "NO_ANSWER", "LOST", "ACTIVE", "WAITING", "WELDED",     "MISSING"};
const char* const EVENT_NAMES[] = {"BOOT",  "SETUP_FAIL", "FAULT",    "FAULT_GONE", "REARM",     "MODE",
                                   "LINK",  "BATTERY",    "SNAPSHOT", "DROPPED",    "TABLE_FULL", "LOG_MOUNT", "CRASH_DUMP",
                                   "CRASH_SAVED", "DTC_CLEARED"};
const char* const RESET_NAMES[] = {"POWER_ON", "SOFTWARE", "PANIC", "WATCHDOG", "BROWNOUT", "OTHER"};

static_assert(sizeof(COMPONENT_NAMES) / sizeof(*COMPONENT_NAMES) == (size_t)Component::Count, "a name per component");
static_assert(sizeof(SYMPTOM_NAMES) / sizeof(*SYMPTOM_NAMES) == (size_t)Symptom::Count, "a name per symptom");
static_assert(sizeof(EVENT_NAMES) / sizeof(*EVENT_NAMES) == (size_t)Event::Count, "a name per event");

template <size_t N>
const char* nameAt(const char* const (&names)[N], size_t i) {
  return i < N ? names[i] : "?";
}

const char* batteryName(uint8_t s) {
  switch ((BatteryState)s) {
    case BatteryState::Ok: return "OK";
    case BatteryState::Low: return "LOW";
    case BatteryState::Cutoff: return "CUTOFF";
    case BatteryState::SensorFault: return "SENSOR";
  }
  return "?";
}

Category eventCategory(Event e) {
  switch (e) {
    case Event::Fault:
    case Event::FaultGone:
    case Event::Rearm: return Category::Safety;
    case Event::ModeChange: return Category::Mode;
    case Event::LinkChange: return Category::Link;
    case Event::Battery: return Category::Power;
    default: return Category::Sys;
  }
}

const char* faultBitName(uint8_t bit) { return bit < 16 ? faultName((FaultMask)(1u << bit)) : "?"; }

}  // namespace

LogRecord makeRecord(Event e, Severity s, uint16_t boot, uint32_t tMs, uint8_t mode, FaultMask faults) {
  LogRecord r;
  memset(&r, 0, sizeof(r));
  r.boot = boot;
  r.tMs = tMs;
  r.sevCat = (uint8_t)(((uint8_t)s << 5) | (uint8_t)eventCategory(e));
  r.event = (uint8_t)e;
  r.mode = mode;
  r.faults = faults;
  return r;
}

int16_t scaled(float value, float scale) {
  float v = value * scale;
  if (!(v == v)) return 0;  // NaN
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)(v < 0 ? v - 0.5f : v + 0.5f);
}

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;  // CRC-16/CCITT-FALSE
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

void seal(LogRecord& r, uint32_t seq) {
  r.seq = seq;
  r.crc = crc16(reinterpret_cast<const uint8_t*>(&r), offsetof(LogRecord, crc));
}

bool intact(const LogRecord& r) { return r.crc == crc16(reinterpret_cast<const uint8_t*>(&r), offsetof(LogRecord, crc)); }

uint8_t faultBit(FaultMask f) {
  for (uint8_t i = 0; i < 16; i++)
    if (f & (1u << i)) return i;
  return 0xFF;
}

const char* severityName(Severity s) {
  switch (s) {
    case Severity::Crit: return "CRIT";
    case Severity::Err: return "ERR";
    case Severity::Warn: return "WARN";
    case Severity::Notice: return "NOTICE";
    case Severity::Info: return "INFO";
  }
  return "?";
}

const char* categoryName(Category c) { return nameAt(CATEGORY_NAMES, (size_t)c); }
const char* componentName(Component c) { return nameAt(COMPONENT_NAMES, (size_t)c); }
const char* symptomName(Symptom s) { return nameAt(SYMPTOM_NAMES, (size_t)s); }
const char* eventName(Event e) { return nameAt(EVENT_NAMES, (size_t)e); }
const char* resetReasonName(ResetReason r) { return nameAt(RESET_NAMES, (size_t)r); }

size_t formatRecord(const LogRecord& r, char* out, size_t outLen) {
  if (outLen == 0) return 0;
  size_t n = 0;
  auto add = [&](int written) {
    if (written > 0) n += (size_t)written;
    if (n >= outLen) n = outLen - 1;
  };
  add(snprintf(out, outLen, "#%lu b%u +%lu.%03lus %s %s %s", (unsigned long)r.seq, (unsigned)r.boot,
               (unsigned long)(r.tMs / 1000), (unsigned long)(r.tMs % 1000), severityName(severityOf(r)),
               categoryName(categoryOf(r)), eventName((Event)r.event)));
  const char* comp = componentName((Component)r.comp);
  switch ((Event)r.event) {
    case Event::Boot:
      add(snprintf(out + n, outLen - n, " reset=%s", resetReasonName((ResetReason)r.code)));
      break;
    case Event::SetupFail: add(snprintf(out + n, outLen - n, " comp=%s", comp)); break;
    case Event::Fault:
      add(snprintf(out + n, outLen - n, " %s comp=%s sym=%s v=%.2f lim=%.2f", faultBitName(r.code), comp,
                   symptomName((Symptom)r.sym), r.v[0] / 100.0, r.v[1] / 100.0));
      break;
    case Event::FaultGone:
      add(snprintf(out + n, outLen - n, " %s comp=%s lasted_s=%.1f", faultBitName(r.code), comp, r.v[0] / 10.0));
      break;
    case Event::Rearm: add(snprintf(out + n, outLen - n, " cleared=0x%04X", (unsigned)(uint16_t)r.v[0])); break;
    case Event::ModeChange:
      add(snprintf(out + n, outLen - n, " from=%s to=%s", modeName((Mode)r.v[0]), modeName((Mode)r.v[1])));
      break;
    case Event::LinkChange:
      add(snprintf(out + n, outLen - n, " state=%s lq=%d", linkName((LinkState)r.code), r.v[0]));
      break;
    case Event::Battery:
      add(snprintf(out + n, outLen - n, " state=%s bat_V=%.2f", batteryName(r.code), r.v[0] / 100.0));
      break;
    case Event::Snapshot:
      add(snprintf(out + n, outLen - n, " bat_V=%.2f I_A=%.2f out=%.2f lq=%d", r.v[0] / 100.0, r.v[1] / 100.0,
                   r.v[2] / 100.0, r.v[3]));
      break;
    case Event::Dropped: add(snprintf(out + n, outLen - n, " n=%d", r.v[0])); break;
    case Event::TableFull: add(snprintf(out + n, outLen - n, " %s comp=%s", faultBitName(r.code), comp)); break;
    case Event::LogMount:
      add(snprintf(out + n, outLen - n, " records=%u torn=%u ready_sectors=%u%s", (unsigned)(uint16_t)r.v[0],
                   (unsigned)(uint16_t)r.v[1], (unsigned)(uint16_t)r.v[2], r.code ? " formatted=1" : ""));
      break;
    case Event::CrashDump:
      add(snprintf(out + n, outLen - n, " pc=0x%08lX",
                   (unsigned long)(((uint32_t)(uint16_t)r.v[0] << 16) | (uint16_t)r.v[1])));
      break;
    case Event::CrashSaved:
      if (r.code == 1)
        add(snprintf(out + n, outLen - n, " cause=FAULT %s", faultBitName((uint8_t)r.v[0])));
      else
        add(snprintf(out + n, outLen - n, " cause=RESET %s", resetReasonName((ResetReason)r.v[0])));
      add(snprintf(out + n, outLen - n, " samples=%d crash_boot=%u", r.v[1], (unsigned)(uint16_t)r.v[2]));
      break;
    case Event::DtcCleared: add(snprintf(out + n, outLen - n, " rows=%d", r.v[0])); break;
    default: break;
  }
  add(snprintf(out + n, outLen - n, " mode=%s faults=0x%04X", modeName((Mode)r.mode), (unsigned)r.faults));
  return n;
}

bool RecordQueue::push(const LogRecord& r) {
  if (count_ < CAPACITY) {
    items_[(head_ + count_) % CAPACITY] = r;
    count_++;
    return true;
  }
  // Full: find the least severe queued record, the newest among equals.
  int victim = -1;
  uint8_t worst = 0;
  for (int i = count_ - 1; i >= 0; i--) {
    uint8_t s = items_[(head_ + i) % CAPACITY].sevCat >> 5;
    if (victim < 0 || s > worst) {
      victim = i;
      worst = s;
    }
  }
  dropped_++;
  // The new record is the newest, so it goes when it is no more severe than the worst queued one.
  if ((uint8_t)(r.sevCat >> 5) >= worst) return false;
  for (int i = victim; i < count_ - 1; i++) items_[(head_ + i) % CAPACITY] = items_[(head_ + i + 1) % CAPACITY];
  items_[(head_ + count_ - 1) % CAPACITY] = r;
  return false;
}

bool RecordQueue::pop(LogRecord& out) {
  if (count_ == 0) return false;
  out = items_[head_];
  head_ = (head_ + 1) % CAPACITY;
  count_--;
  return true;
}

uint32_t RecordQueue::takeDropped() {
  uint32_t d = dropped_;
  dropped_ = 0;
  return d;
}

}  // namespace rideon
