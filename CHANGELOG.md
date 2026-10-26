# Changelog

## 3.0.0 (in progress)

A fresh start as a PlatformIO project, built on the v2.2 motor class and the [code review](docs/reviews/2026-10-firmware-code-tour.md).

- New layout: a single `src/main.cpp` entry point, `include/pins.h` and `include/config.h`, and the hardware-free safety core in `lib/rideon_core` with PC unit tests.
- Radio channels are 1-based, matching AlfredoCRSF. v2 read channel 0, which is past the array.
- The link is judged by packet age (200 ms), the receiver link flag and the link quality, never by channel values.
- Faults latch and clear only by re-arming from Locked with the sticks centred and the pedal released.
- GPIO21 is a real kill line: LOW at boot until PWM is zeroed, and dropped on every hard fault and the kill switch.
- Contactor hold on GPIO12 and steering enable on GPIO2.
- Pin audit against the ESP32-WROOM-32D datasheet and errata: wheel-speed inputs move to GPIO34/35 (GPIO36/39 glitch when the ADC runs), pedal to GPIO36, battery sense to GPIO39, DS18B20 on GPIO15. New [PINOUT.md](docs/hardware/PINOUT.md) describes every pin. Pin rules run as static_asserts in every build, with a unit test and a docs consistency check in CI.
- New pin map: the shared pins are gone, and RR_RPWM is off strapping pin 12.
- Current sensing goes through an ADS1115 at the real 8500:1 ratio, is active in every mode, and treats IS saturation as a driver fault.
- Out-of-range battery readings are a fault, not "OK". Low-voltage cutoff now has hysteresis.
- Separate acceleration and braking ramps, no reverse until stopped, and reverse capped at 50 %.
- Fixed 100 Hz loop, task watchdog, reset reason logged, and a serial log that never blocks.
- Docs: draft schematic v0.2, safety-standards review, audio plan, and a CLAUDE.md for Claude Code.
- Bench firmwares `radio_check` (phase 2) and `one_motor` (phase 4).
- Schematic v0.3 in KiCad 9 (docs/hardware/kicad): a one-page system wiring overview, net names, connector pinouts, a checked wire list with gauge, colour and fuse per wire, and a BOM matched to the 24 Oct order. Safety changes from v0.2: both E-stop NC blocks in the contactor coil, an enable pull-down at every driver, a protected pedal supply, fuses on the 12 V outputs. Plus a v2-vs-v3 hardware comparison.
- One BOM for the project (docs/hardware/kicad/BOM.md and .csv): one numbered line per distinct part, with category, manufacturer / part number, status, order line and the cost of the 24 Oct order; also the tools, materials and spares. docs/hardware/BOM.md now points there.
- The pioarduino platform for Arduino core 3.x. AlfredoCRSF is pinned to a commit. The MCP23017 libraries are dropped.
- The v2 docs, sketches and PDFs moved to `archive/`.
- New `ESTOP` fault: the E-stop sense input locks the car and holds the contactor open, so releasing the E-stop with the pedal held no longer restarts it.
- New `CUR_SENS` fault: every ADS1115 transfer is checked, and a current channel with no good reading for 250 ms stops the car instead of keeping its last value. The Adafruit ADS1X15 and BusIO libraries are dropped.
- `DRIVER` waits 300 ms like `OVERCUR`, so start-up inrush no longer trips it, and the steering board's IS line is checked too.
- The `one_motor` bench firmware has the task watchdog.
- RC frames are found with the same resync as AlfredoCRSF, so a stray byte no longer hides a real frame and trips `LINK`.
- Firmware matched to schematic v0.3:
  - Current sense now uses half the per-pin IS resistor (`IS_PIN_RESISTOR_OHMS`), since R_IS and L_IS are joined. Before this, current read about half the real value, so the 20 A limit really sat near 40 A. The `DRIVER` level comes from the resistor (1.8 V with 1k boards) instead of a fixed 3.2 V.
  - `DRIVER` and `OVERCUR` open the contactor, because a shorted driver MOSFET ignores its enable. The drive enable now drops before the contactor opens.
  - The motor bus is read on ADS1115 #2 A2 (new R5-R7 in the schematic, with a bleed resistor). At boot the contactor stays open until the bus has drained; a bus still live after 10 s is the new permanent `K1_WELD` fault. A contactor held closed with no bus is the new `NO_BUS` fault.
  - I2C runs at 100 kHz, the PCF8574's rating, instead of 400 kHz.
  - The box fans run whenever the ESP32 runs (PCA9685 channel 1). The DS18B20 is read, and a box above 65 °C is the new `HOT` fault.
  - `WHEEL_REVERSED` in `config.h` flips any wheel whose motor turns backwards.
- New 3D exploded view of the car and the electronics box (tools/exploded-view, React Three Fiber), generated from the BOM, the wire list and the schematic nets: realistic and blueprint styles, show/hide by part, info cards with upstream and downstream wiring, variant A/B swap, revision history, and GLB/glTF/PNG/PDF/JSON export.

## 2.2 (2026)

An OOP refactor of 2.1: a `BTS7960Motor` class with signed speed and ramping through zero. It compiled, but kept every safety bug from 2.1. It also dropped about 60 to 80 lines of warnings and checks, and read current only in REMOTE mode. Archived in `archive/v2.2/`.

## 2.1 (2025)

Pin reallocation for 24 V, plus CRSF at 420000 baud, a "thermal FAULT" input and current sensing on the IS pins. It never compiled: four IS pin `#define`s were commented out but still used. Archived in `archive/v2.1/`.

## 2.0 and earlier

See [archive/docs-v2/CHANGELOG.md](archive/docs-v2/CHANGELOG.md).
