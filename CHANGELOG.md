# Changelog

## 3.0.0 (in progress)

A fresh start as a PlatformIO project, built on the v2.2 motor class and the [code review](docs/reviews/2026-10-firmware-code-tour.md).

- New layout: a single `src/main.cpp` entry point, `include/pins.h` and `include/config.h`, and the hardware-free safety core in `lib/rideon_core` with PC unit tests.
- Radio channels are 1-based, matching AlfredoCRSF. v2 read channel 0, which is past the array.
- The link is judged by packet age (200 ms), the receiver link flag and the link quality, never by channel values.
- Faults latch and clear only by re-arming from Locked with the sticks centred and the pedal released.
- GPIO21 is a real kill line: LOW at boot until PWM is zeroed, and dropped on every hard fault and the kill switch.
- Contactor hold on GPIO12 and steering enable on GPIO2.
- New pin map: the shared pins are gone, and RR_RPWM is off strapping pin 12.
- Current sensing goes through an ADS1115 at the real 8500:1 ratio, is active in every mode, and treats IS saturation as a driver fault.
- Out-of-range battery readings are a fault, not "OK". Low-voltage cutoff now has hysteresis.
- Separate acceleration and braking ramps, no reverse until stopped, and reverse capped at 50 %.
- Fixed 100 Hz loop, task watchdog, reset reason logged, and a serial log that never blocks.
- Bench firmwares `radio_check` (phase 2) and `one_motor` (phase 4).
- The pioarduino platform for Arduino core 3.x. AlfredoCRSF is pinned to a commit. The MCP23017 libraries are dropped.
- The v2 docs, sketches and PDFs moved to `archive/`.

## 2.2 (2026)

An OOP refactor of 2.1: a `BTS7960Motor` class with signed speed and ramping through zero. It compiled, but kept every safety bug from 2.1. It also dropped about 60 to 80 lines of warnings and checks, and read current only in REMOTE mode. Archived in `archive/v2.2/`.

## 2.1 (2025)

Pin reallocation for 24 V, plus CRSF at 420000 baud, a "thermal FAULT" input and current sensing on the IS pins. It never compiled: four IS pin `#define`s were commented out but still used. Archived in `archive/v2.1/`.

## 2.0 and earlier

See [archive/docs-v2/CHANGELOG.md](archive/docs-v2/CHANGELOG.md).
