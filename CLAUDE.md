# CLAUDE.md

Notes for Claude Code (in Cursor or in the cloud) working on this repo. Read [README.md](README.md) and [SAFETY.md](SAFETY.md) first; this file only adds what they don't say.

## What this is

ESP32 firmware for a 24 V Can-Am Maverick X3 ride-on (4WD: four drive motors, plus a steering motor). The child drives with the pedal; a parent drives or overrides with an ExpressLRS radio over CRSF. **No phone app, Wi-Fi or Bluetooth**, ever.

Priorities, in order: **1. safety, 2. smooth driving, 3. everything else.** When a feature and a safety rule disagree, the rule wins.

Status: v3 has never driven a car. Only bench checks so far. Follow [docs/COMMISSIONING.md](docs/COMMISSIONING.md) phase by phase; no child rides before phase 6 passes.

## Build, test, flash

PlatformIO project (pioarduino platform, Arduino core 3.x). Libraries come from `lib_deps` in `platformio.ini`, not from Arduino IDE's library manager.

| Env | What it is |
| --- | --- |
| `car` | The car firmware, `src/main.cpp` |
| `radio_check` | Phase 2 bench: radio link and channels only |
| `one_motor` | Phase 4 bench: one motor on the front-left driver |
| `native` | Unit tests for `lib/rideon_core` and the pin map, run on the PC |

```sh
pio test -e native                 # unit tests
python tools/check_pin_docs.py     # pins.h, PINOUT.md and WIRING.md agree
pio run -e car                     # build
pio run -e car -t upload           # flash over USB
pio device monitor                 # serial, 115200 baud, exception decoder on
```

On Windows, if `pio` is not on PATH, use the PlatformIO terminal in Cursor or `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`. Add `--upload-port COMx` / `--port COMx` if more than one board is plugged in.

**Before flashing a board that is wired into the car: key off (contactor open, drivers unpowered) and wheels off the ground.** See [docs/hardware/DEVICE_FIRMWARE.md](docs/hardware/DEVICE_FIRMWARE.md). Never flash with the car powered just because the code "should" keep the drivers off.

## Code layout and rules

- `lib/rideon_core/` holds safety and control logic with no hardware calls. New logic goes here when it can, with a test in `test/test_<name>/`.
- `src/` is hardware glue; `src/bench/` holds the bench firmwares. `include/config.h` holds every limit and tunable; `include/pins.h` is the pin map.
- **Pins:** `include/pins.h`, [docs/hardware/PINOUT.md](docs/hardware/PINOUT.md) and [docs/hardware/WIRING.md](docs/hardware/WIRING.md) change together in the same PR. `include/pin_rules.h` static-asserts the rules (no GPIO used twice, no outputs on 34–39, analog on ADC1 only, strapping pins handled) and the CI `pins` job checks the docs match.
- Faults latch until the parent re-arms from Locked; never add a fault that clears itself. Link loss is judged by the CRSF link state, never by channel values. Fault codes: [docs/firmware/FAULT_CODES.md](docs/firmware/FAULT_CODES.md).
- Changing a `config.h` limit means re-running the commissioning gates that depend on it; say which in the PR.
- Firmware design: [docs/firmware/ARCHITECTURE.md](docs/firmware/ARCHITECTURE.md), modes: [CONTROL_MODES.md](docs/firmware/CONTROL_MODES.md), radio channels: [CHANNEL_MAP.md](docs/firmware/CHANNEL_MAP.md) and [radio/](radio/).
- `archive/` is the old v2 Arduino code, reference only. Don't build on it.

## Workflow

- `main` is protected. Every change goes through a branch and a PR, with CI green (`pins`, `test`, and builds of `car`, `radio_check`, `one_motor`). Shai reviews and merges; don't merge yourself.
- Add a line to [CHANGELOG.md](CHANGELOG.md) for user-visible or safety-relevant changes.
- Tracking lives in GitHub issues: phase parents #6 (phase 1) to #12 (phase 7), task issues under them, labelled `phase-N`, `safety`, `firmware`, `hardware`, `radio`, `waiting-on-car`. Reference the issue in the PR.

## Open bench checks

Left over from PR #49; they need real hardware and can't be checked in CI:

- **E-stop sense polarity:** confirm the ESTOP fault (bit 9) reads the right way with the E-stop pressed and released.
- **CUR_SENS unplug test:** unplug the ADS1115 (or its I2C lines) while running and confirm the CUR_SENS fault (bit 10) trips and brakes within about 250 ms.
- Also still open: I2C bus recovery (#36), and a pedal shorted high reads as full pedal (waits on #16).
