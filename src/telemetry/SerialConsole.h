// The serial console on USB (115200 baud): type a command and press Enter.
//
//   help              the commands
//   status            mode, link, faults, battery, log and fault-table size
//   log [n]           the newest n log records from flash, oldest first (default 50)
//   dtc               the fault table, with both freeze frames of each row
//   crash             the crash recorder's last kept copy: 5 s before a crash or hard fault
//   clear dtc yes     clear the fault table (car Locked or Killed and stopped)
//   test watchdog     hang the loop so the watchdog resets the ESP32 (commissioning,
//                     car Locked or Killed and stopped)
//
// Nothing here can arm, drive or change a limit. Output is paced: a few
// lines per tick, only while the serial buffer has room, so a long listing
// never stalls the control loop. Parsing and line text are in
// rideon_core/Console (tested on the PC).
#pragma once

#include <stdint.h>

#include "Faults.h"
#include "LinkMonitor.h"
#include "ModeMachine.h"

namespace console {

struct Status {
  rideon::Mode mode;
  rideon::LinkState link;
  uint8_t linkQuality;
  rideon::FaultMask latched;
  float batteryV;
  float busV;
  float driveAmps;
  float output;
  bool safeToStall;  // Locked or Killed, drivers off, stopped
};

// Call once per tick: reads typed characters, runs a finished command, and
// prints the next few lines of a listing.
void update(const Status& s);
// True while a listing is being printed (main.cpp holds its status line).
bool busy();

}  // namespace console
