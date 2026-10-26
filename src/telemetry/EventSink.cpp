#include "EventSink.h"

#include <Arduino.h>
#include <Preferences.h>

#include "telemetry/Log.h"

using namespace rideon;

namespace events {

namespace {
RecordQueue queue;
FaultTable table;
Recorder recorder;
uint32_t seq = 0;
}  // namespace

void begin(ResetReason reason) {
  uint16_t boot = 0;
  Preferences prefs;
  if (prefs.begin("evlog", false)) {
    boot = (uint16_t)(prefs.getUShort("boot", 0) + 1);
    prefs.putUShort("boot", boot);
    prefs.end();
  } else {
    logx::line("NVS not available: boot number unknown (0)");
  }
  recorder.begin(boot, reason, millis(), queue);
}

void setupFailed(Component comp) { recorder.setupFailed(comp, millis(), queue, table); }

void update(const RecorderInputs& in) { recorder.update(in, queue, table); }

void drain(int maxRecords) {
  LogRecord r;
  char line[160];
  // A record waits in the queue rather than being dropped by a full serial buffer.
  for (int i = 0; i < maxRecords && logx::hasRoom(sizeof(line) + 2) && queue.pop(r); i++) {
    seal(r, seq++);
    formatRecord(r, line, sizeof(line));
    logx::line("%s", line);
  }
}

const FaultTable& faultTable() { return table; }
uint16_t bootNumber() { return recorder.boot(); }

}  // namespace events
