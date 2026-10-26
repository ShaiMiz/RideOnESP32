// Owns the event log, the fault table and the recorder for the car
// firmware, and prints records to serial.
//
// The control loop calls update() and drain() once per tick. update() only
// queues records; drain() prints a few per tick so the serial port never
// stalls the loop. Flash storage of records and the fault table comes next
// (docs/firmware/LOG_EVENTS.md).
#pragma once

#include "EventLog.h"
#include "FaultTable.h"
#include "Recorder.h"

namespace events {

// Bumps the boot counter in NVS and queues the boot record.
void begin(rideon::ResetReason reason);
void setupFailed(rideon::Component comp);
void update(const rideon::RecorderInputs& in);
// Print up to maxRecords queued records.
void drain(int maxRecords);

const rideon::FaultTable& faultTable();
uint16_t bootNumber();

}  // namespace events
