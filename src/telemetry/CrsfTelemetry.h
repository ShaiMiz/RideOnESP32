// Telemetry to the T-Lite screen through the receiver: battery voltage and
// current, mode, fault code. (later: features)
#pragma once

class AlfredoCRSF;

class CrsfTelemetry {
 public:
  void update(AlfredoCRSF&, float /*volts*/, float /*amps*/, const char* /*status*/) {}
};
