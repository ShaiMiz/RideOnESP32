#include "Faults.h"

namespace rideon {

const char* faultName(FaultMask mask) {
  if (mask & FAULT_LINK_LOST) return "LINK";
  if (mask & FAULT_KILL_SWITCH) return "KILL";
  if (mask & FAULT_BATTERY_LOW) return "BATT_LOW";
  if (mask & FAULT_BATTERY_SENSOR) return "BATT_SENS";
  if (mask & FAULT_OVERCURRENT) return "OVERCUR";
  if (mask & FAULT_DRIVER) return "DRIVER";
  if (mask & FAULT_INPUT) return "INPUT";
  if (mask & FAULT_TILT) return "TILT";
  if (mask & FAULT_SETUP) return "SETUP";
  if (mask & FAULT_ESTOP) return "ESTOP";
  if (mask & FAULT_CURRENT_SENSE) return "CUR_SENS";
  return "OK";
}

}  // namespace rideon
