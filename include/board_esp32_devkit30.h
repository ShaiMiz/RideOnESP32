// What each GPIO on our board can and cannot do.
//
// Board: 30-pin "ESP32 DevKit V1" style, ESP32-WROOM-32D module, USB-C,
// blue LED on GPIO2 (silkscreen "D2"), BOOT and EN buttons. Checked against
// Shai's photos of the board (Desktop\ss, 17 Oct 2026) and against:
//   ESP32-WROOM-32D datasheet, pin definitions and strapping pins
//     https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf
//   ESP32 series datasheet, IO_MUX, ADC and RTC GPIO tables
//     https://documentation.espressif.com/esp32_datasheet_en.pdf
//   ESP32 errata, "GPIO36 and GPIO39 inputs pulled down for ~80 ns when SAR ADC is powered"
//     https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32/03-errata-description/esp32/gpio-inputs-pulled-down.html
//
// Header layout, USB connector at the bottom, chip side up:
//   left  (top to bottom): EN 36(VP) 39(VN) 34 35 32 33 25 26 27 14 12 13 GND VIN
//   right (top to bottom): 23 22 1(TX0) 3(RX0) 21 19 18 5 17 16 4 2 15 GND 3V3
// That is 25 usable GPIOs plus the USB serial pair 1/3. GPIO0 (BOOT button)
// and GPIO6-11 (SPI flash) are not on the header.
#pragma once

#include <stdint.h>

namespace board {

// GPIOs that reach a header pin.
constexpr bool onHeader(uint8_t g) {
  return g == 1 || g == 2 || g == 3 || g == 4 || g == 5 || (g >= 12 && g <= 19) || (g >= 21 && g <= 23) ||
         (g >= 25 && g <= 27) || (g >= 32 && g <= 36) || g == 39;
}

// Wired to the module's SPI flash. Touching them crashes the chip.
constexpr bool flash(uint8_t g) { return g >= 6 && g <= 11; }

// UART0, wired to the USB serial chip: flashing and the serial monitor.
constexpr bool usbSerial(uint8_t g) { return g == 1 || g == 3; }

// GPIO34-39: input only, no output driver and no internal pull-up or pull-down.
constexpr bool inputOnly(uint8_t g) { return g >= 34 && g <= 39; }

// ADC1 channels. ADC2 cannot be read while the radio (Wi-Fi) is on, so we use ADC1 only.
constexpr bool adc1(uint8_t g) { return g >= 32 && g <= 39; }

// Strapping pins, sampled at reset:
//   GPIO0  boot mode (BOOT button, not on header)
//   GPIO2  must be LOW or floating to enter download mode; on-board LED to GND
//   GPIO5  SDIO slave timing; internal pull-up, idles HIGH
//   GPIO12 MTDI, flash voltage: HIGH selects 1.8 V and the module will not boot
//   GPIO15 MTDO, boot log on/off; internal pull-up, idles HIGH
constexpr bool strapping(uint8_t g) { return g == 0 || g == 2 || g == 5 || g == 12 || g == 15; }

// Strapping pins that must be LOW at reset. A signal held LOW by an external
// pull-down agrees with them, which is why the kill lines live here.
constexpr bool mustBeLowAtBoot(uint8_t g) { return g == 2 || g == 12; }

// Strapping pins that idle HIGH at reset (internal pull-up).
constexpr bool highAtBoot(uint8_t g) { return g == 5 || g == 15; }

// Pins that toggle or output a signal during the ROM boot, before our code runs.
// Never put a kill line here.
constexpr bool activeDuringBoot(uint8_t g) { return g == 1 || g == 3 || g == 5 || g == 14 || g == 15; }

// GPIO36/39 read a false LOW for ~80 ns every time the SAR ADC powers up
// (errata above). Do not count pulses or take interrupts on them.
constexpr bool adcGlitch(uint8_t g) { return g == 36 || g == 39; }

// LEDC: 16 PWM channels in total.
constexpr int PWM_CHANNELS = 16;

}  // namespace board
