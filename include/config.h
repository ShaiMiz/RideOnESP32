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
constexpr float IS_RATIO = 8500.0f;  // load current / IS current, datasheet typical
// VERIFY (#15): the resistor from each IS pin to GND on your IBT-2 boards.
constexpr float IS_PIN_RESISTOR_OHMS = 1000.0f;
// R_IS and L_IS are joined in each driver's control plug (WIRING.md), so the
// board's two IS resistors sit in parallel: the sense line sees half of one.
// Using the single value here would read half the real current.
constexpr float IS_SENSE_OHMS = IS_PIN_RESISTOR_OHMS / 2.0f;
// A BTS7960 flags a fault by sourcing its IS saturation current, 4-7 mA by the
// datasheet. Into IS_SENSE_OHMS the smallest of that may stay well under the
// 3.3 V clamp, so the fault level is 90 % of what 4 mA makes, capped just
// under the clamp. With 1k boards that is 1.8 V.
constexpr float IS_FAULT_MIN_A = 0.004f;
constexpr float IS_CLAMP_VOLTS = 3.2f;
constexpr float IS_FAULT_VOLTS =
    IS_FAULT_MIN_A * IS_SENSE_OHMS * 0.9f < IS_CLAMP_VOLTS ? IS_FAULT_MIN_A * IS_SENSE_OHMS * 0.9f : IS_CLAMP_VOLTS;
constexpr float MOTOR_CURRENT_LIMIT_A = 20.0f;
constexpr float TOTAL_CURRENT_LIMIT_A = 35.0f;  // below the 40 A main fuse
constexpr uint32_t OVERCURRENT_HOLD_MS = 300;   // allow start-up inrush
// Normal load also crosses IS_FAULT_VOLTS above IS_FAULT_VOLTS / IS_SENSE_OHMS
// * IS_RATIO (about 31 A with 1k boards), so a high IS line is only a driver
// fault once it stays there. A real fault keeps the line pinned (the driver
// latches it), and the driver has already switched itself off, so waiting
// costs nothing.
constexpr uint32_t DRIVER_FAULT_HOLD_MS = 300;
constexpr uint32_t ADS_CONVERSION_TIMEOUT_MS = 20;  // one conversion takes about 1.2 ms at 860 SPS
constexpr uint32_t CURRENT_SENSE_STALE_MS = 250;    // each channel normally refreshes every 60 ms

// ---- Motor bus and contactor (MOTOR_24V on ADS1115 #2 A2) -----------------------
constexpr float BUS_DIVIDER = (150.0f + 12.0f) / 12.0f;  // 150k / 12k, as the battery divider
constexpr float BUS_OPEN_MAX_FRACTION = 0.3f;    // K1 open and the caps drained: bus below 30 % of the battery
constexpr float BUS_CLOSED_MIN_FRACTION = 0.8f;  // K1 closed: bus above 80 % of the battery
// 2.2k bleed and 5 x 470 uF: a 5.2 s time constant, so a full bus falls under
// 30 % in about 6.3 s. After a quick key-off/key-on the car waits that long.
constexpr uint32_t BUS_DISCHARGE_TIMEOUT_MS = 10000;
constexpr uint32_t BUS_CLOSE_TIME_MS = 300;  // K1 closes in tens of ms; the caps charge at once

// ---- Box temperature (DS18B20 on GPIO15) and fans (PCA9685 channel 1) -------------
constexpr float BOX_TEMP_LIMIT_C = 65.0f;    // VERIFY on the bench: well under the 85 C parts in the box
constexpr float BOX_TEMP_RECOVER_C = 55.0f;  // must cool below this before HOT can clear
constexpr uint32_t BOX_TEMP_HOLD_MS = 5000;
constexpr uint32_t BOX_TEMP_STALE_MS = 5000;
constexpr uint32_t BOX_TEMP_PERIOD_MS = 1000;  // one reading a second (a conversion takes 750 ms)
constexpr bool BOX_TEMP_REQUIRED = false;      // set true once the DS18B20 is fitted: then a lost sensor is HOT
constexpr uint32_t FAN_REFRESH_MS = 1000;      // fans are re-commanded on every second, in case the PCA9685 reset

// ---- Wheel direction ---------------------------------------------------------------
// true flips a wheel whose motor runs backwards for a positive command (it is
// mounted mirrored, or its plug is wired the other way). Set once in phase 5,
// one wheel at a time, wheels off the ground. Order: FL, FR, RL, RR.
constexpr bool WHEEL_REVERSED[4] = {false, false, false, false};

// ---- I2C -------------------------------------------------------------------------
constexpr uint32_t I2C_HZ = 100000;      // the PCF8574 is rated for 100 kHz; the bus also runs out to the dash and steering
constexpr uint16_t I2C_TIMEOUT_MS = 5;  // a stuck bus fails a transfer fast instead of stalling the loop

// ---- Event log (docs/firmware/LOG_EVENTS.md) ----------------------------------------
// Flash sectors kept erased ahead of the log's write position: 8 x 128 records
// can be written while driving before any erase is needed. Erasing stalls both
// cores, so it only happens at boot and while the car is stopped with the
// drivers off.
constexpr uint32_t LOG_READY_SECTORS = 8;
constexpr int LOG_RECORDS_PER_TICK = 2;  // records written to flash and serial per loop tick

}  // namespace cfg
