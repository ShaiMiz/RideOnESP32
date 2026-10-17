// Pin map for the 30-pin ESP32-WROOM-32D board.
// This is the only place pins are defined. docs/hardware/WIRING.md copies it.
// Source: the hardware plan, "Pin map (30-pin board, four drive motors)".
#pragma once

#include <stdint.h>

namespace pins {

// Drive motors, one IBT-2 (BTS7960) board per wheel
constexpr uint8_t FL_RPWM = 25;
constexpr uint8_t FL_LPWM = 26;
constexpr uint8_t FR_RPWM = 27;
constexpr uint8_t FR_LPWM = 14;
constexpr uint8_t RL_RPWM = 32;
constexpr uint8_t RL_LPWM = 33;
constexpr uint8_t RR_RPWM = 13;  // moved off strapping pin 12
constexpr uint8_t RR_LPWM = 4;

// Steering motor, fifth IBT-2 board
constexpr uint8_t STEER_RPWM = 18;
constexpr uint8_t STEER_LPWM = 19;

// Kill lines. Each has an external 10k pull-down so it reads "off" during
// boot, reset, flashing and crashes.
constexpr uint8_t DRIVE_EN = 21;       // R_EN + L_EN of all four drive boards
constexpr uint8_t STEER_EN = 2;        // R_EN + L_EN of the steering board
constexpr uint8_t CONTACTOR_HOLD = 12; // MOSFET in series with the contactor coil; strapping pin, must be LOW at boot

// ExpressLRS receiver (Jumper AION RX Mini), CRSF on UART2
constexpr uint8_t CRSF_RX = 16;  // ESP32 RX <- receiver TX
constexpr uint8_t CRSF_TX = 17;  // ESP32 TX -> receiver RX

// I2C bus: ADS1115 x2, AS5600, MPU-6050, OLED, PCF8574, PCA9685
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t I2C_SDA = 23;

// Analog inputs (ADC1 only, so they work with Wi-Fi off or on)
constexpr uint8_t PEDAL = 34;
constexpr uint8_t BATTERY_SENSE = 35;  // 150k / 12k divider

// Wheel-speed hall sensors, external 10k pull-ups (GPIO36/39 have none)
constexpr uint8_t WHEEL_SPEED_L = 36;
constexpr uint8_t WHEEL_SPEED_R = 39;

// Sound module serial TX
constexpr uint8_t SOUND_TX = 5;

// Spare
constexpr uint8_t SPARE = 15;

}  // namespace pins

// I2C addresses
namespace i2c {
constexpr uint8_t ADS_DRIVE = 0x48;  // ADS1115 #1: FL, FR, RL, RR current sense
constexpr uint8_t ADS_AUX = 0x49;    // ADS1115 #2: steering current sense (A0), A1-A3 spare
constexpr uint8_t PCF8574 = 0x20;    // shifter, horn, E-stop sense, seat switch
constexpr uint8_t AS5600 = 0x36;
constexpr uint8_t MPU6050 = 0x68;
constexpr uint8_t OLED = 0x3C;
constexpr uint8_t PCA9685 = 0x40;
}  // namespace i2c

// PCF8574 inputs (bit numbers). Switches pull to GND, so 0 = pressed.
namespace pcf {
constexpr uint8_t SHIFT_FORWARD = 0;
constexpr uint8_t SHIFT_REVERSE = 1;
constexpr uint8_t HORN = 2;
constexpr uint8_t ESTOP_SENSE = 3;  // reads the E-stop state; the E-stop itself cuts power in hardware
constexpr uint8_t SEAT = 4;         // optional seat switch
}  // namespace pcf
