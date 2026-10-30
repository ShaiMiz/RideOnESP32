#include "Console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EventLog.h"
#include "ModeMachine.h"

namespace rideon {

namespace {

// Next space-separated word of `s`, lower-cased into `word`. Returns the rest.
const char* nextWord(const char* s, char* word, size_t len) {
  while (*s == ' ' || *s == '\t') s++;
  size_t n = 0;
  while (*s && *s != ' ' && *s != '\t') {
    if (n + 1 < len) word[n++] = (char)tolower((unsigned char)*s);
    s++;
  }
  word[n] = 0;
  return s;
}

size_t clampLen(int written, size_t outLen) {
  if (written < 0) return 0;
  return (size_t)written < outLen ? (size_t)written : outLen - 1;
}

}  // namespace

ParsedCommand parseCommand(const char* line) {
  char w1[16], w2[16], w3[16];
  const char* rest = nextWord(line, w1, sizeof(w1));
  rest = nextWord(rest, w2, sizeof(w2));
  rest = nextWord(rest, w3, sizeof(w3));
  char extra[4];
  nextWord(rest, extra, sizeof(extra));
  bool more = extra[0] != 0;

  if (!w1[0]) return {Command::None, 0};
  if ((!strcmp(w1, "help") || !strcmp(w1, "?")) && !w2[0]) return {Command::Help, 0};
  if (!strcmp(w1, "status") && !w2[0]) return {Command::Status, 0};
  if (!strcmp(w1, "dtc") && !w2[0]) return {Command::Dtc, 0};
  if (!strcmp(w1, "crash") && !w2[0]) return {Command::Crash, 0};
  if (!strcmp(w1, "report") && !w2[0]) return {Command::Report, 0};
  if (!strcmp(w1, "log") && !w3[0]) {
    if (!w2[0]) return {Command::Log, LOG_DEFAULT_COUNT};
    char* end = nullptr;
    unsigned long n = strtoul(w2, &end, 10);
    if (*end || n == 0) return {Command::Unknown, 0};
    return {Command::Log, n > 100000UL ? 100000U : (uint32_t)n};
  }
  if (!strcmp(w1, "clear") && !strcmp(w2, "dtc") && !more) {
    if (!w3[0]) return {Command::ClearDtcAsk, 0};
    if (!strcmp(w3, "yes")) return {Command::ClearDtc, 0};
  }
  if (!strcmp(w1, "test") && !strcmp(w2, "watchdog") && !w3[0]) return {Command::TestWatchdog, 0};
  return {Command::Unknown, 0};
}

bool LineReader::feed(char c) {
  if (c == '\n' && lastCr_) {  // the LF of a CRLF
    lastCr_ = false;
    return false;
  }
  lastCr_ = c == '\r';
  if (c == '\r' || c == '\n') {
    memcpy(done_, buf_, len_);
    done_[len_] = 0;
    len_ = 0;
    return true;
  }
  if ((c == '\b' || c == 0x7F) && len_ > 0) {
    len_--;
    return false;
  }
  if (isprint((unsigned char)c) && len_ < MAX) buf_[len_++] = c;
  return false;
}

size_t faultList(FaultMask mask, char* out, size_t outLen) {
  if (outLen == 0) return 0;
  out[0] = 0;
  if (mask == FAULT_NONE) return clampLen(snprintf(out, outLen, "none"), outLen);
  size_t n = 0;
  for (int bit = 0; bit < 16; bit++) {
    FaultMask f = (FaultMask)(1u << bit);
    if (!(mask & f)) continue;
    n += clampLen(snprintf(out + n, outLen - n, "%s%s", n ? "," : "", faultName(f)), outLen - n);
  }
  return n;
}

size_t formatFaultRow(const FaultRow& row, char* out, size_t outLen) {
  if (outLen == 0) return 0;
  return clampLen(
      snprintf(out, outLen, "%s %s x%u %s worst=%.2f lim=%.2f first=b%u+%.1fs last=b%u+%.1fs%s",
               faultName((FaultMask)(1u << (row.faultBit & 15))), componentName((Component)row.comp),
               (unsigned)row.count, symptomName((Symptom)row.sym), row.worst / 100.0, row.limit / 100.0,
               (unsigned)row.first.boot, row.first.tMs / 1000.0, (unsigned)row.last.boot, row.last.tMs / 1000.0,
               (row.status & DTC_FAILING_NOW) ? " now" : ""),
      outLen);
}

size_t formatFreezeFrame(const FreezeFrame& f, char* out, size_t outLen) {
  if (outLen == 0) return 0;
  char box[8];
  if (f.boxC == -128)
    snprintf(box, sizeof(box), "?");
  else
    snprintf(box, sizeof(box), "%dC", f.boxC);
  return clampLen(snprintf(out, outLen,
                           "b%u +%lu.%03lus %s lq=%u cmd=%.2f out=%.2f pedal=%.2f bat=%.2fV bus=%.2fV "
                           "I=%.2f,%.2f,%.2f,%.2f,%.2fA box=%s",
                           (unsigned)f.boot, (unsigned long)(f.tMs / 1000), (unsigned long)(f.tMs % 1000),
                           modeName((Mode)f.mode), (unsigned)f.linkQuality, f.command / 100.0, f.output / 100.0,
                           f.pedal / 100.0, f.batteryV / 100.0, f.busV / 100.0, f.amps[0] / 100.0,
                           f.amps[1] / 100.0, f.amps[2] / 100.0, f.amps[3] / 100.0, f.amps[4] / 100.0, box),
                  outLen);
}

bool ReportTrigger::update(bool hornHeld, bool parked, uint32_t nowMs) {
  if (!hornHeld) {
    holding_ = false;
    blocked_ = false;
    return false;
  }
  if (blocked_) return false;  // this press already fired, or was not parked: wait for release
  if (!parked) {
    holding_ = false;
    blocked_ = true;
    return false;
  }
  if (!holding_) {
    holding_ = true;
    startMs_ = nowMs;
  }
  if ((uint32_t)(nowMs - startMs_) < HOLD_MS) return false;
  holding_ = false;
  blocked_ = true;
  return true;
}

}  // namespace rideon
