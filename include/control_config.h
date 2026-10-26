// The car's ControlConfig, built from config.h. src/main.cpp and the PC
// tests (test/test_controller) both use this one function, so the tests
// judge exactly the values the car runs.
#pragma once

#include "Controller.h"
#include "config.h"

namespace cfg {

inline rideon::ControlConfig carControl() {
  return {
      {cfg::CH_MIN_VALID, cfg::CH_MAX_VALID, cfg::CH_LOW, cfg::CH_CENTER, cfg::CH_HIGH, cfg::STICK_DEADBAND_US,
       cfg::SWITCH_LOW_BELOW, cfg::SWITCH_HIGH_ABOVE},
      {cfg::LINK_TIMEOUT_MS, cfg::LQ_DEGRADED_BELOW, cfg::LQ_LOST_BELOW},
      {cfg::CH_CENTER, cfg::ARM_CENTER_TOLERANCE_US, cfg::PEDAL_RELEASED_FRACTION},
      {cfg::ACCEL_PER_S, cfg::BRAKE_PER_S, cfg::STOPPED_EPSILON},
      {cfg::KID_ACCEL_PER_S, cfg::BRAKE_PER_S, cfg::STOPPED_EPSILON},
      {{cfg::BATTERY_SENSOR_MIN_V, cfg::BATTERY_SENSOR_MAX_V, cfg::BATTERY_LOW_WARN_V, cfg::BATTERY_CUTOFF_V,
        cfg::BATTERY_RECOVER_V, cfg::BATTERY_HIGH_WARN_V, cfg::BATTERY_CUTOFF_HOLD_MS},
       cfg::MOTOR_CURRENT_LIMIT_A,
       cfg::TOTAL_CURRENT_LIMIT_A,
       cfg::OVERCURRENT_HOLD_MS,
       cfg::DRIVER_FAULT_HOLD_MS,
       cfg::BOX_TEMP_LIMIT_C},
      {cfg::KID_CAP_MIN, cfg::KID_CAP_MID, cfg::KID_CAP_MAX},
      cfg::PARENT_CAP,
      cfg::REVERSE_CAP,
      cfg::LQ_DEGRADED_CAP,
      cfg::KID_BRAKE_OVERRIDE,
      4,      // all four wheels have current sense
      true,   // steering driver IS line
      true};  // motor bus reading
}

}  // namespace cfg
