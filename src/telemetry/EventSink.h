// Owns the event log, the fault table and the recorder for the car
// firmware: stores records in the `evlog` flash partition, keeps the fault
// table in NVS, and prints every record to serial.
//
// The control loop calls update() and drain() once per tick. update() only
// queues records. drain() writes a few per tick to flash and serial; a
// record waits in the queue while either has no room. Flash sectors are
// erased, and the fault table saved, only when drain() is told the car is
// stopped with the drivers off, because both stall the CPU.
#pragma once

#include "CrashRing.h"
#include "EventLog.h"
#include "FaultTable.h"
#include "LogRing.h"
#include "Recorder.h"

namespace events {

// Call early in setup(), before the watchdog starts: bumps the boot counter,
// mounts the flash log (erasing the first sectors if needed), loads the fault
// table and queues the BOOT, LOG_MOUNT and CRASH_DUMP records. After an
// unexpected reset while driving, it also keeps the crash recorder's last
// 5 s (CRASH_SAVED).
void begin(rideon::ResetReason reason);
void setupFailed(rideon::Component comp);
void update(const rideon::RecorderInputs& in);
// Store and print up to maxRecords queued records. safeToStall: the car is
// stopped with the drivers off, so a flash erase or NVS write may run.
void drain(int maxRecords, bool safeToStall);

// Clear the fault table, save it and log DTC_CLEARED. Writes NVS: call only
// when a stall is safe. Returns the rows cleared.
int clearFaultTable();
// The last crash recorder copy kept (in NVS), or nullptr if none.
const rideon::SavedCrash* lastCrash();

const rideon::FaultTable& faultTable();
rideon::LogRing& flashLog();
uint16_t bootNumber();

}  // namespace events
