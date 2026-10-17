// Pin map for the 30-pin ESP32-WROOM-32D board (on the MRD068A breakout).
//
// This is the only place pins are defined. docs/hardware/PINOUT.md describes
// every pin and docs/hardware/WIRING.md lists what each connects to; CI checks
// both against this file (tools/check_pin_docs.py). The rules at the bottom
// run on every build, so a duplicate or misplaced pin stops the build.
//
// All 25 header GPIOs are in use; GPIO1/3 stay free for USB flashing and the
// serial monitor. See PINOUT.md "Pin budget" before adding anything.
#pragma once

#include <stdint.h>

#include "pin_rules.h"

namespace pins {

// ---- Drive motors: one IBT-2 (BTS7960) per wheel, RPWM = forward, LPWM = reverse.
// Ordinary bidirectional pins, none of them strapping. GPIO14 toggles during
// boot, which is harmless because DRIVE_EN holds every driver off until armed.
constexpr uint8_t FL_RPWM = 25;
constexpr uint8_t FL_LPWM = 26;
constexpr uint8_t FR_RPWM = 27;
constexpr uint8_t FR_LPWM = 14;
constexpr uint8_t RL_RPWM = 32;
constexpr uint8_t RL_LPWM = 33;
constexpr uint8_t RR_RPWM = 13;
constexpr uint8_t RR_LPWM = 4;

// ---- Steering motor: fifth IBT-2. RPWM = right, LPWM = left.
constexpr uint8_t STEER_RPWM = 18;
constexpr uint8_t STEER_LPWM = 19;

// ---- Kill lines. External 10k pull-down on each, so they read "off" during
// boot, reset, flashing and crashes. GPIO2 and GPIO12 are strapping pins that
// must be LOW at reset anyway, so the pull-down serves both jobs.
constexpr uint8_t DRIVE_EN = 21;        // R_EN + L_EN of all four drive boards
constexpr uint8_t STEER_EN = 2;         // R_EN + L_EN of the steering board; on-board blue LED shows it
constexpr uint8_t CONTACTOR_HOLD = 12;  // gate of the MOSFET in series with the contactor coil

// ---- ExpressLRS receiver (Jumper AION RX Mini), CRSF on UART2 (its default pins).
constexpr uint8_t CRSF_RX = 16;  // ESP32 RX <- receiver TX
constexpr uint8_t CRSF_TX = 17;  // ESP32 TX -> receiver RX

// ---- I2C bus: ADS1115 x2, AS5600, MPU-6050, OLED, PCF8574, PCA9685.
// The Arduino default SDA is GPIO21, which DRIVE_EN uses; GPIO23 works the same.
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t I2C_SDA = 23;

// ---- Analog inputs. ADC1 only (ADC2 is blocked while the radio is on).
// On GPIO36/39 because those pins glitch LOW whenever the ADC runs, which
// only matters for edge-counting inputs, not for analog ones.
constexpr uint8_t PEDAL = 36;          // "VP"
constexpr uint8_t BATTERY_SENSE = 39;  // "VN"; 150k / 12k divider, 2.2 V at 29.4 V

// ---- Wheel-speed hall sensors (A3144, open collector). Input-only pins with
// no internal pull-ups, so each has an external 10k pull-up to 3.3 V.
constexpr uint8_t WHEEL_SPEED_L = 34;
constexpr uint8_t WHEEL_SPEED_R = 35;

// ---- Sound module (DFPlayer Mini) serial, TX only. UART idles HIGH, which
// matches strapping pin GPIO5.
constexpr uint8_t SOUND_TX = 5;

// ---- DS18B20 box temperature, 1-Wire. Its 4.7k pull-up keeps strapping pin
// GPIO15 HIGH at boot.
constexpr uint8_t ONEWIRE_TEMP = 15;

using pinrules::PinUse;
using pinrules::Use;

// Every pin above, once. The build checks this list, and the docs check
// compares it with PINOUT.md and WIRING.md.
constexpr PinUse MAP[] = {
    {FL_RPWM, "FL_RPWM", Use::Pwm},
    {FL_LPWM, "FL_LPWM", Use::Pwm},
    {FR_RPWM, "FR_RPWM", Use::Pwm},
    {FR_LPWM, "FR_LPWM", Use::Pwm},
    {RL_RPWM, "RL_RPWM", Use::Pwm},
    {RL_LPWM, "RL_LPWM", Use::Pwm},
    {RR_RPWM, "RR_RPWM", Use::Pwm},
    {RR_LPWM, "RR_LPWM", Use::Pwm},
    {STEER_RPWM, "STEER_RPWM", Use::Pwm},
    {STEER_LPWM, "STEER_LPWM", Use::Pwm},
    {DRIVE_EN, "DRIVE_EN", Use::KillLine},
    {STEER_EN, "STEER_EN", Use::KillLine},
    {CONTACTOR_HOLD, "CONTACTOR_HOLD", Use::KillLine},
    {CRSF_RX, "CRSF_RX", Use::UartRx},
    {CRSF_TX, "CRSF_TX", Use::UartTx},
    {I2C_SCL, "I2C_SCL", Use::I2c},
    {I2C_SDA, "I2C_SDA", Use::I2c},
    {PEDAL, "PEDAL", Use::Analog},
    {BATTERY_SENSE, "BATTERY_SENSE", Use::Analog},
    {WHEEL_SPEED_L, "WHEEL_SPEED_L", Use::Pulse},
    {WHEEL_SPEED_R, "WHEEL_SPEED_R", Use::Pulse},
    {SOUND_TX, "SOUND_TX", Use::UartTx},
    {ONEWIRE_TEMP, "ONEWIRE_TEMP", Use::OneWire},
};
constexpr size_t MAP_SIZE = sizeof(MAP) / sizeof(MAP[0]);

}  // namespace pins

// I2C addresses
namespace i2c {
constexpr uint8_t ADS_DRIVE = 0x48;  // ADS1115 #1 (ADDR to GND): FL, FR, RL, RR current sense
constexpr uint8_t ADS_AUX = 0x49;    // ADS1115 #2 (ADDR to VDD): steering current sense (A0), A1-A3 spare
constexpr uint8_t PCF8574 = 0x20;    // shifter, horn, E-stop sense, seat switch
constexpr uint8_t AS5600 = 0x36;     // steering angle
constexpr uint8_t MPU6050 = 0x68;    // tilt and crash
constexpr uint8_t OLED = 0x3C;       // SSD1306 0.91"
constexpr uint8_t PCA9685 = 0x40;    // accessory PWM; also answers the 0x70 all-call address
constexpr uint8_t ALL[] = {ADS_DRIVE, ADS_AUX, PCF8574, AS5600, MPU6050, OLED, PCA9685, 0x70};
}  // namespace i2c

// PCF8574 inputs (bit numbers). Switches pull to GND, so 0 = pressed.
namespace pcf {
constexpr uint8_t SHIFT_FORWARD = 0;
constexpr uint8_t SHIFT_REVERSE = 1;
constexpr uint8_t HORN = 2;
constexpr uint8_t ESTOP_SENSE = 3;  // reads the E-stop state; the E-stop itself cuts power in hardware
constexpr uint8_t SEAT = 4;         // optional seat switch
constexpr uint8_t ALL[] = {SHIFT_FORWARD, SHIFT_REVERSE, HORN, ESTOP_SENSE, SEAT};  // P5-P7 spare
}  // namespace pcf

// ---- Build-time checks. A failure here names the rule; run
// `pio test -e native -f test_pins` to see which pin breaks it.
namespace pinrules {
static_assert(firstDuplicate(pins::MAP, pins::MAP_SIZE) < 0, "pins.h: a GPIO is used twice");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, onBoard) < 0,
              "pins.h: a GPIO is not on the 30-pin header, is a flash pin (6-11) or is the USB serial pair (1/3)");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, canDrive) < 0,
              "pins.h: an output is on an input-only pin (34-39)");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, analogOnAdc1) < 0,
              "pins.h: an analog input is not on ADC1 (32-39)");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, pulseNotOnGlitchPin) < 0,
              "pins.h: a pulse input is on GPIO36/39, which glitch when the ADC runs");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, killLineQuietAtBoot) < 0,
              "pins.h: a kill line is on a pin that is HIGH or toggles during boot");
static_assert(firstBreaking(pins::MAP, pins::MAP_SIZE, strappingUseMatches) < 0,
              "pins.h: a strapping pin (2, 5, 12, 15) carries a signal that fights its boot level");
static_assert(countUse(pins::MAP, pins::MAP_SIZE, Use::Pwm) <= board::PWM_CHANNELS, "pins.h: more PWM outputs than LEDC channels");
static_assert(firstRepeat(i2c::ALL, sizeof(i2c::ALL)) < 0, "pins.h: two I2C devices share an address");
static_assert(firstRepeat(pcf::ALL, sizeof(pcf::ALL)) < 0, "pins.h: two PCF8574 inputs share a bit");
static_assert(pcf::SEAT < 8 && pcf::ESTOP_SENSE < 8 && pcf::HORN < 8 && pcf::SHIFT_REVERSE < 8 && pcf::SHIFT_FORWARD < 8,
              "pins.h: PCF8574 has bits 0-7 only");
}  // namespace pinrules
