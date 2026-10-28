// Telemetry to the T-Lite screen through the receiver: battery voltage,
// drive current, charge used, and the mode with the latched fault. The frames
// and their timing come from rideon::TelemetryScheduler; this only writes
// them when the UART has room. See radio/MODEL_SETUP.md for the EdgeTX side.
#pragma once

#include "CrsfTelemetry.h"
#include "input/RadioInput.h"

class RadioTelemetry {
 public:
  void update(RadioInput& radio, const rideon::TelemetryInputs& in) {
    uint8_t buf[rideon::CRSF_TELEMETRY_MAX];
    uint8_t len = scheduler_.update(in, buf);
    if (len > 0 && radio.sendFrame(buf, len)) scheduler_.sent();
  }

 private:
  rideon::TelemetryScheduler scheduler_;
};
