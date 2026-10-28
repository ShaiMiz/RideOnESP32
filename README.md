[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

# RideOnESP32

An ESP32 controller for a 24 V ride-on car, driven by a parent's ExpressLRS radio or by the child's pedal, with safety first and smooth driving second.

> **Not yet safe to ride.** Version 3 has never driven a car. No child rides until every gate in [docs/COMMISSIONING.md](docs/COMMISSIONING.md) up to phase 6 has passed. Read [SAFETY.md](SAFETY.md) first.

## The car and the hardware

- **Car:** Can-Am Maverick X3 24 V, 4WD (four drive motors) with a steering motor.
- **Controller:** ESP32-WROOM-32D, 30-pin, on an MRD068A screw-terminal breakout.
- **Drivers:** five BTS7960 (IBT-2) boards, one per wheel plus one for steering.
- **Radio:** Jumper T-Lite V2 (EdgeTX, ELRS 2.4 GHz) and a Jumper AION RX Mini receiver on CRSF. There is no phone app, and the ESP32's Wi-Fi and Bluetooth stay off. A separate listen-only Bluetooth module lets a phone read the serial log ([BLUETOOTH.md](docs/hardware/BLUETOOTH.md)).
- **Power:** 40 A main fuse, key switch, red E-stop and a contactor that cut motor power in hardware, whatever the ESP32 does.

Details: [docs/hardware/](docs/hardware/). How to update the firmware on each device: [DEVICE_FIRMWARE.md](docs/hardware/DEVICE_FIRMWARE.md). The full reasoning lives in the [hardware plan](https://claude.ai/code/artifact/17bcaa28-e72d-40de-8aa5-4d734f5fe4ad).

## How it drives

| Mode | Who drives | Set by |
| --- | --- | --- |
| Locked | Nobody; drivers off | Mode switch low |
| Kid | Pedal and shifter, at the speed cap the parent sets | Mode switch middle |
| Parent | Radio throttle (steering comes with phase 5) | Mode switch high |
| Killed | Nobody; drivers and contactor off | Kill switch |

The parent always outranks the child: pulling the stick back in Kid mode brakes the car. Losing the radio link stops the car in every mode. See [docs/firmware/CONTROL_MODES.md](docs/firmware/CONTROL_MODES.md).

## Status

| Part | State |
| --- | --- |
| Safety core (modes, arming, fault latch, link loss, ramps, battery) | Written and unit-tested on the PC |
| `radio_check` bench firmware (phase 2) | Complete |
| `one_motor` bench firmware (phase 4) | Complete, not yet bench-tested |
| `bt_setup` bench firmware (sets up the Bluetooth log module) | Complete, not yet bench-tested |
| `car` firmware: drive, power monitoring, kill lines | Written, not yet bench-tested |
| Steering, IMU, wheel speed, OLED, lights and sound | Stubs |

## Build

This is a [PlatformIO](https://platformio.org/) project. It uses the pioarduino platform for Arduino core 3.x.

```sh
pio test -e native        # safety logic and pin map tests, on the PC
python3 tools/check_pin_docs.py  # pins.h, PINOUT.md and WIRING.md agree
pio run -e radio_check    # phase 2 bench firmware
pio run -e one_motor      # phase 4 bench firmware
pio run -e bt_setup       # one-time Bluetooth log module setup
pio run -e car            # the car firmware
pio run -e car -t upload  # flash over USB
pio device monitor        # serial log, 115200 baud
```

CI (`.github/workflows/build.yml`) runs on every push and pull request: the pin docs check, all unit tests, and a build of every firmware. The pin rules in `include/pin_rules.h` run inside every build, so a GPIO used twice or put on the wrong kind of pin stops the build. See [docs/hardware/PINOUT.md](docs/hardware/PINOUT.md).

## Layout

```
include/        pins.h (the pin map, checked on every build), config.h (every limit and tunable)
lib/rideon_core safety and control logic with no hardware calls, unit-tested
src/            hardware modules and main.cpp; src/bench/ holds the bench firmwares
test/           unit tests for lib/rideon_core and the pin map
tools/          CI helper scripts
radio/          T-Lite model and ELRS setup
docs/           commissioning, hardware, firmware, reviews
archive/        the v2 sketches and docs, kept for reference only
```

Start with [docs/firmware/ARCHITECTURE.md](docs/firmware/ARCHITECTURE.md) for the firmware and [docs/COMMISSIONING.md](docs/COMMISSIONING.md) for the build.

## History

Versions 2.1 and 2.2 were Arduino sketches. A code review ([docs/reviews/2026-10-firmware-code-tour.md](docs/reviews/2026-10-firmware-code-tour.md)) found that their failsafe could not detect a lost radio link. It also found off-by-one radio channels, faults that re-armed themselves, and a hardware enable that was never dropped. Version 3 is a fresh start built around those findings. The old files are in [archive/](archive/).
