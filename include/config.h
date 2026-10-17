// Every tunable in one place. Values marked VERIFY depend on hardware facts
// that are still open (see docs/hardware/CAR.md) and must be checked on the
// bench before the car carries a child.
#pragma once

#include <stdint.h>

namespace cfg {

// ---- Loop ------------------------------------------------------------------
constexpr uint32_t LOOP_HZ = 100;
constexpr uint32_t LOOP_PERIOD_US = 1000000UL / LOOP_HZ;
constexpr uint32_t WATCHDOG_MS = 300;  // task watchdog: a stuck loop resets the ESP32, the pull-downs stop the car

// ---- Motor PWM ---------------------------------------------------------------
constexpr uint32_t PWM_FREQ_HZ = 16000;  // above hearing; check driver temperature on the bench
constexpr uint8_t PWM_BITS = 10;

// ---- Ramps (fraction of full speed per second) -------------------------------
constexpr float ACCEL_PER_S = 0.5f;      // 0 to full in 2 s
constexpr float KID_ACCEL_PER_S = 0.35f; // gentler in Kid mode
constexpr float BRAKE_PER_S = 2.0f;      // full to 0 in 0.5 s, also used for link loss and soft faults
constexpr float REVERSE_CAP = 0.5f;      // reverse is never faster than half of the forward cap
constexpr float STOPPED_EPSILON = 0.01f; // below this the car counts as stopped for direction changes

// ---- Speed caps ----------------------------------------------------------------
constexpr float KID_CAP_MIN = 0.3f;   // speed-cap switch low
constexpr float KID_CAP_MID = 0.6f;   // speed-cap switch middle
constexpr float KID_CAP_MAX = 1.0f;   // speed-cap switch high
constexpr float PARENT_CAP = 1.0f;
constexpr float KID_BRAKE_OVERRIDE = -0.25f;  // parent pulls the stick back past this in Kid mode: brake, pedal ignored

// ---- Radio (CRSF channel values are microseconds) -----------------------------
constexpr uint16_t CH_MIN_VALID = 900;
constexpr uint16_t CH_MAX_VALID = 2100;
constexpr uint16_t CH_LOW = 988;
constexpr uint16_t CH_CENTER = 1500;
constexpr uint16_t CH_HIGH = 2012;
constexpr uint16_t STICK_DEADBAND_US = 30;
constexpr uint16_t ARM_CENTER_TOLERANCE_US = 60;  // sticks must be this close to centre to arm
constexpr uint16_t SWITCH_LOW_BELOW = 1300;
constexpr uint16_t SWITCH_HIGH_ABOVE = 1700;

// ---- Link ------------------------------------------------------------------------
constexpr uint32_t LINK_TIMEOUT_MS = 200;     // no RC packet for this long = link lost
constexpr uint8_t LQ_DEGRADED_BELOW = 70;     // link quality % below this: speed cap halved
constexpr uint8_t LQ_LOST_BELOW = 40;         // below this: treated as lost
constexpr float LQ_DEGRADED_CAP = 0.5f;

// ---- Pedal ---------------------------------------------------------------------
constexpr uint16_t PEDAL_RELEASED_MV = 400;   // VERIFY: stock pedal may be a switch, not a pot
constexpr uint16_t PEDAL_FULL_MV = 2600;
constexpr float PEDAL_RELEASED_FRACTION = 0.05f;

// ---- Battery (24 V pack, chemistry unknown: VERIFY) -----------------------------
constexpr float BATTERY_DIVIDER = (150.0f + 12.0f) / 12.0f;  // 150k / 12k
constexpr float BATTERY_SENSOR_MIN_V = 5.0f;   // below: wire off or divider broken -> fault
constexpr float BATTERY_SENSOR_MAX_V = 32.0f;  // above: reading impossible -> fault
constexpr float BATTERY_HIGH_WARN_V = 27.5f;   // BTS7960 operating limit; it locks out 27.6-30 V
constexpr float BATTERY_LOW_WARN_V = 22.0f;
constexpr float BATTERY_CUTOFF_V = 21.0f;      // lead-acid 1.75 V/cell
constexpr float BATTERY_RECOVER_V = 22.5f;     // hysteresis: must rest above this before a cutoff can clear
constexpr uint32_t BATTERY_CUTOFF_HOLD_MS = 2000;  // ignore sag shorter than this

// ---- Current sensing (BTS7960 IS pin through ADS1115) ---------------------------
constexpr float IS_RATIO = 8500.0f;        // load current / IS current, datasheet typical
constexpr float IS_RESISTOR_OHMS = 1000.0f; // VERIFY: read the resistor on your IBT-2 boards
constexpr float IS_FAULT_VOLTS = 3.2f;     // IS pinned at the 3.3 V clamp = driver fault flag (4-7 mA)
constexpr float MOTOR_CURRENT_LIMIT_A = 20.0f;
constexpr float TOTAL_CURRENT_LIMIT_A = 35.0f;  // below the 40 A main fuse
constexpr uint32_t OVERCURRENT_HOLD_MS = 300;   // allow start-up inrush
// With a 1k IS resistor, about 27 A of normal load also reaches the 3.3 V clamp,
// so a clamped IS line is only a driver fault once it stays there. A real fault
// keeps the line pinned (the driver latches it), and the driver has already
// switched itself off, so waiting costs nothing.
constexpr uint32_t DRIVER_FAULT_HOLD_MS = 300;

}  // namespace cfg
