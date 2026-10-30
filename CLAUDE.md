# CLAUDE.md

Notes for Claude Code (in Cursor or in the cloud) working on this repo. Read [README.md](README.md) and [SAFETY.md](SAFETY.md) first; this file only adds what they don't say.

## What this is

ESP32 firmware for a 24 V Can-Am Maverick X3 ride-on (4WD: four drive motors, plus a steering motor). The child drives with the pedal; a parent drives or overrides with an ExpressLRS radio over CRSF. **No phone app, and the ESP32's Wi-Fi and Bluetooth stay off**, ever. The one exception Shai chose (26 Oct 2026) is a separate listen-only Bluetooth module that hears the serial log, with its TXD unconnected ([BLUETOOTH.md](docs/hardware/BLUETOOTH.md)).

Priorities, in order: **1. safety, 2. smooth driving, 3. everything else.** When a feature and a safety rule disagree, the rule wins.

Status: v3 has never driven a car. Only bench checks so far. Follow [docs/COMMISSIONING.md](docs/COMMISSIONING.md) phase by phase; no child rides before phase 6 passes.

## Hardware decisions so far

- **Plug and play:** the new electronics go in one removable box that plugs into the stock harness plugs and fits where the stock controller sits under the seat. The car must stay able to go back to stock (spare HY2005RX controller). If the harness pairs motors, use a reversible splitter adapter at the motor plugs, never cut the stock wiring (#33).
- **Per-wheel control stays:** four independent drive channels, for e-diff, traction control and per-motor fault detection. Variant A is 4+1 IBT-2 boards; variant B is 2x Cytron MDD20A, more compact, with a 74LVC08 PWM gate and external current sensors (#32, undecided).
- **Parts:** Shai orders from AliExpress (2–3 weeks shipping); local Israeli shops for urgent safety parts (fuse, contactor, E-stop, wire). Plan purchases so one AliExpress order covers a phase (#28).
- **Waiting on Shai at the car:** photos of every stock controller plug (#17), box and under-seat measurements (#18), battery label and chemistry (#13), motor labels (#14), IBT-2 sense resistor (#15), pedal type (#16). Don't guess these values in `config.h`; leave the TODO and point to the issue.
- The full reasoning is in the [hardware plan](https://claude.ai/code/artifact/17bcaa28-e72d-40de-8aa5-4d734f5fe4ad).

## Plans and reviews

- Schematic v0.3 in KiCad: [docs/hardware/kicad/](docs/hardware/kicad/), with CONNECTORS.md, WIRE_LIST.md, BOM.md (matched to the 24 Oct order) and NETS.md. BOM.md is the one list of every part, tool and material (owned, ordered, to buy); add new shopping items to `gen/tables.py` (TOOLS, BUY_LINKS), not to separate shopping files. Generated: edit `gen/design.py` and `gen/wiring.py`, run `python3 docs/hardware/kicad/gen/generate.py`, then `docs/hardware/kicad/check.sh` (KiCad ERC + netlist check via Docker). The ESP32 nets come from pins.h, so a pin change means regenerating; CI fails if the committed files are stale. J1-J8 (car-facing) stay TBD until #17. The SVG draft v0.2 in [docs/hardware/schematic/](docs/hardware/schematic/) is superseded but kept.
- 3D exploded view: [tools/exploded-view/](tools/exploded-view/) (React Three Fiber). Its parts and wiring come from BOM.csv, WIRE_LIST.csv and design.py via `npm run data`; placement is hand-set in `src/data/layout.js`. After a BOM, wire list or pin change, regenerate `src/data/project.json`, and place any new ref (`npm test` fails on unplaced parts).
- v2 vs v3 hardware comparison: [docs/reviews/2026-10-v2-vs-v3-hardware.md](docs/reviews/2026-10-v2-vs-v3-hardware.md).
- Safety-standards review: [docs/reviews/2026-10-safety-standards.md](docs/reviews/2026-10-safety-standards.md), EN 71-1 4.15 and EN 62115 (Annex J, remote-controlled ride-ons) against this design. Its 8 gaps are not yet filed as issues; Shai decides which to take on.
- Audio plan: [docs/hardware/AUDIO.md](docs/hardware/AUDIO.md). Bluetooth is on the music amp only, never the ESP32.
- Firmware code review of v2: [docs/reviews/2026-10-firmware-code-tour.md](docs/reviews/2026-10-firmware-code-tour.md).

## Build, test, flash

PlatformIO project (pioarduino platform, Arduino core 3.x). Libraries come from `lib_deps` in `platformio.ini`, not from Arduino IDE's library manager.

| Env | What it is |
| --- | --- |
| `car` | The car firmware, `src/main.cpp` |
| `radio_check` | Phase 2 bench: radio link and channels only |
| `one_motor` | Phase 4 bench: one motor on the front-left driver |
| `bt_setup` | Bench: sets the listen-only Bluetooth log module to 115200 baud, once |
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

- `lib/rideon_core/` holds safety and control logic with no hardware calls. New logic goes here when it can, with a test in `test/test_<name>/`. Every decision that can move or stop the car goes through `rideon::Controller`; `src/main.cpp` only reads hardware into it and writes its outputs.
- Verification: [docs/VERIFICATION.md](docs/VERIFICATION.md). A new or changed safety rule gets its SR- row, a test naming the ID, and a mutant in `tools/mutation_check.py` if a one-line slip could break it. CI job `verify` runs sanitizers, coverage floors, cppcheck and the mutation check; locally `tools/run_native_tests.sh` (needs g++, Linux or WSL). Never lower a coverage floor or delete a mutant to get green.
- `src/` is hardware glue; `src/bench/` holds the bench firmwares. `include/config.h` holds every limit and tunable; `include/pins.h` is the pin map.
- **Pins:** `include/pins.h`, [docs/hardware/PINOUT.md](docs/hardware/PINOUT.md) and [docs/hardware/WIRING.md](docs/hardware/WIRING.md) change together in the same PR, and the KiCad schematic is regenerated. `include/pin_rules.h` static-asserts the rules (no GPIO used twice, no outputs on 34–39, analog on ADC1 only, strapping pins handled) and the CI `pins` job checks the docs match.
- Faults latch until the parent re-arms from Locked; never add a fault that clears itself. Link loss is judged by the CRSF link state, never by channel values. Fault codes: [docs/firmware/FAULT_CODES.md](docs/firmware/FAULT_CODES.md).
- Changing a `config.h` limit means re-running the commissioning gates that depend on it; say which in the PR.
- Logging: a new fault, event, part or symptom needs its row in [docs/firmware/LOG_EVENTS.md](docs/firmware/LOG_EVENTS.md) (CI `pins` job runs `tools/check_log_events.py`). The loop only queues records; flash erases and NVS writes happen only with the car stopped and the drivers off. A change to `LogRecord` or `FaultRow` layout must bump `FaultTable::FORMAT` or start a new log format.
- Firmware design: [docs/firmware/ARCHITECTURE.md](docs/firmware/ARCHITECTURE.md), modes: [CONTROL_MODES.md](docs/firmware/CONTROL_MODES.md), radio channels: [CHANNEL_MAP.md](docs/firmware/CHANNEL_MAP.md) and [radio/](radio/).
- `archive/` is the old v2 Arduino code, reference only. Don't build on it.

## Workflow

- `main` is protected. Every change goes through a branch and a PR, with CI green (`pins`, `test`, `verify`, and builds of `car`, `radio_check`, `one_motor`, `bt_setup`). Shai reviews and merges; don't merge yourself.
- Claude in Shai's claude.ai project also opens PRs here from the cloud, and chats aren't shared. The repo, PRs and issues are the shared record: `git pull` and check open PRs before starting, and write decisions into an issue or doc rather than leaving them only in chat.
- Add a line to [CHANGELOG.md](CHANGELOG.md) for user-visible or safety-relevant changes.
- Tracking lives in GitHub issues: phase parents #6 (phase 1) to #12 (phase 7), task issues under them, labelled `phase-N`, `safety`, `firmware`, `hardware`, `radio`, `waiting-on-car`. Reference the issue in the PR.

## Open bench checks

Left over from PR #49; they need real hardware and can't be checked in CI:

- **E-stop sense polarity:** confirm the ESTOP fault (bit 9) reads the right way with the E-stop pressed and released.
- **CUR_SENS unplug test:** unplug the ADS1115 (or its I2C lines) while running and confirm the CUR_SENS fault (bit 10) trips and brakes within about 250 ms.
- Also still open: I2C bus recovery (#36), and a pedal shorted high reads as full pedal (waits on #16).

From the firmware-vs-schematic fixes (contactor checks, fans, box temperature):

- **Contactor checks:** the `K1_WELD` and `NO_BUS` gates in phase 3 need R5-R7 fitted.
- **DS18B20 timing:** the 1-Wire code in `src/sensing/BoxTemp.cpp` is our own bit-banging and has not run on hardware yet. Confirm good readings (no `box=?`) before setting `BOX_TEMP_REQUIRED`.
- **D2 TVS:** driver and over-current faults now open the contactor, maybe on a rolling car. Fit D2 once the battery voltage is known (#13), before phase 6.
