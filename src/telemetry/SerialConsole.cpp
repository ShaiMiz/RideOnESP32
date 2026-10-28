#include "SerialConsole.h"

#include <Arduino.h>

#include "Console.h"
#include "EventLog.h"
#include "telemetry/EventSink.h"
#include "telemetry/Log.h"

using namespace rideon;

namespace console {

namespace {

constexpr int READ_PER_TICK = 32;   // typed characters handled per tick
constexpr int LINES_PER_TICK = 3;   // at 115200 baud, about 70 lines a second fit
constexpr int LINE_ROOM = 200;      // a line is printed only when this much fits

enum class Job : uint8_t { None, Help, Log, Dtc, Crash };

LineReader reader;
ReportTrigger reportTrigger;
Job job = Job::None;
uint32_t index = 0;  // next line of the job
uint32_t total = 0;  // log: records to print

// A report runs these jobs one after the other.
struct Queued {
  Job job;
  uint32_t lines;
};
Queued queue[3];
int queued = 0;
int queueNext = 0;

const char* const HELP[] = {
    "commands:",
    "  status            mode, link, faults, battery, log and fault-table size",
    "  log [n]           newest n log records, oldest first (default 50)",
    "  dtc               fault table, with first and last freeze frame of each row",
    "  crash             crash recorder: 5 s before the last crash or hard fault while driving",
    "  report            status, dtc, crash and log 50 in one go (also: hold the horn 3 s with the car parked)",
    "  clear dtc yes     clear the fault table (car Locked or Killed and stopped)",
    "  test watchdog     hang the loop so the watchdog resets the ESP32 (car Locked or Killed and stopped)",
};
constexpr uint32_t HELP_LINES = sizeof(HELP) / sizeof(*HELP);

// The job's line number `i` into `buf`. Returns false when the job is done.
bool jobLine(uint32_t i, char* buf, size_t len) {
  switch (job) {
    case Job::Help:
      if (i >= HELP_LINES) return false;
      snprintf(buf, len, "%s", HELP[i]);
      return true;
    case Job::Log: {
      if (i >= total) return false;
      LogRecord r;
      if (!events::flashLog().newest(total - 1 - i, r)) {
        snprintf(buf, len, "(record not readable)");
        return true;
      }
      formatRecord(r, buf, len);
      return true;
    }
    case Job::Dtc: {
      const FaultTable& t = events::faultTable();
      if (t.size() == 0) {
        if (i > 0) return false;
        snprintf(buf, len, "fault table empty");
        return true;
      }
      uint32_t row = i / 3;
      if (row >= (uint32_t)t.size()) return false;
      const FaultRow& r = t.at((int)row);
      size_t n = 0;
      switch (i % 3) {
        case 0: formatFaultRow(r, buf, len); break;
        case 1: n = (size_t)snprintf(buf, len, "  first: "); formatFreezeFrame(r.first, buf + n, len - n); break;
        default: n = (size_t)snprintf(buf, len, "  last:  "); formatFreezeFrame(r.last, buf + n, len - n); break;
      }
      return true;
    }
    case Job::Crash: {
      const SavedCrash* c = events::lastCrash();
      if (!c) {
        if (i > 0) return false;
        snprintf(buf, len, "no crash kept");
        return true;
      }
      if (i == 0) {
        if (c->cause == CRASH_FAULT)
          snprintf(buf, len, "crash: hard fault %s in boot %u, %u samples, 100 ms apart, oldest first",
                   faultName((FaultMask)(1u << (c->code & 15))), (unsigned)c->boot, (unsigned)c->count);
        else
          snprintf(buf, len, "crash: %s reset in boot %u, %u samples, 100 ms apart, oldest first",
                   resetReasonName((ResetReason)c->code), (unsigned)c->boot, (unsigned)c->count);
        return true;
      }
      if (i > c->count) return false;
      formatFreezeFrame(c->samples[i - 1], buf, len);
      return true;
    }
    case Job::None: break;
  }
  return false;
}

void start(Job j, uint32_t lines = 0) {
  job = j;
  index = 0;
  total = lines;
}

// The current job is done: start the next queued one, if any.
void next() {
  job = Job::None;
  if (queueNext < queued) {
    start(queue[queueNext].job, queue[queueNext].lines);
    queueNext++;
  }
}

void clearQueue() {
  queued = 0;
  queueNext = 0;
}

void printStatus(const Status& s) {
  char faults[96];
  faultList(s.latched, faults, sizeof(faults));
  logx::line("boot=%u up=%lus mode=%s link=%s lq=%u faults=%s", (unsigned)events::bootNumber(),
             (unsigned long)(millis() / 1000), modeName(s.mode), linkName(s.link), (unsigned)s.linkQuality, faults);
  logx::line("bat=%.2fV bus=%.2fV I=%.2fA out=%.2f", s.batteryV, s.busV, s.driveAmps, s.output);
  LogRing& ring = events::flashLog();
  logx::line("log: %s records=%lu next=#%lu room=%lu, fault table rows=%d, crash kept=%s",
             ring.mounted() ? "flash" : "serial only", (unsigned long)ring.count(), (unsigned long)ring.nextSeq(),
             (unsigned long)ring.room(), events::faultTable().size(), events::lastCrash() ? "yes" : "no");
}

// Status now, then the fault table, the crash and the newest log records.
void report(const Status& s) {
  logx::line("==== report ====");
  printStatus(s);
  clearQueue();
  queue[queued++] = {Job::Dtc, 0};
  queue[queued++] = {Job::Crash, 0};
  LogRing& ring = events::flashLog();
  uint32_t n = ring.mounted() ? (LOG_DEFAULT_COUNT < ring.count() ? LOG_DEFAULT_COUNT : ring.count()) : 0;
  if (n > 0) queue[queued++] = {Job::Log, n};
  next();
}

void run(const ParsedCommand& p, const Status& s) {
  switch (p.cmd) {
    case Command::None: return;
    case Command::Help: start(Job::Help); return;
    case Command::Status: printStatus(s); return;
    case Command::Report: report(s); return;
    case Command::Log: {
      LogRing& ring = events::flashLog();
      if (!ring.mounted()) {
        logx::line("no flash log (evlog partition missing)");
        return;
      }
      uint32_t n = p.arg < ring.count() ? p.arg : ring.count();
      logx::line("last %lu of %lu records:", (unsigned long)n, (unsigned long)ring.count());
      start(Job::Log, n);
      return;
    }
    case Command::Dtc: start(Job::Dtc); return;
    case Command::Crash: start(Job::Crash); return;
    case Command::ClearDtcAsk: logx::line("to clear the fault table type: clear dtc yes"); return;
    case Command::ClearDtc:
      if (!s.safeToStall) {
        logx::line("refused: the car must be Locked or Killed and stopped");
        return;
      }
      logx::line("fault table cleared (%d rows)", events::clearFaultTable());
      return;
    case Command::TestWatchdog:
      if (!s.safeToStall) {
        logx::line("refused: the car must be Locked or Killed and stopped");
        return;
      }
      logx::line("hanging the loop: the watchdog should reset the ESP32 and the next boot print BOOT reset=WATCHDOG");
      Serial.flush();
      for (;;) {
      }
    case Command::Unknown: logx::line("unknown command, type help"); return;
  }
}

}  // namespace

void update(const Status& s) {
  for (int i = 0; i < READ_PER_TICK && Serial.available() > 0; i++) {
    if (reader.feed((char)Serial.read())) {
      job = Job::None;  // a new command stops a listing
      clearQueue();
      run(parseCommand(reader.line()), s);
    }
  }
  if (reportTrigger.update(s.hornHeld, s.parked, millis())) {
    job = Job::None;
    report(s);
  }
  char line[LINE_ROOM];
  for (int i = 0; i < LINES_PER_TICK && job != Job::None; i++) {
    if (!logx::hasRoom(LINE_ROOM)) break;
    if (!jobLine(index, line, sizeof(line))) {
      next();
      continue;
    }
    logx::line("%s", line);
    index++;
  }
}

bool busy() { return job != Job::None; }

}  // namespace console
