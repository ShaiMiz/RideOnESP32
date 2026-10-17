# Archive

These are the version 2 files, kept unchanged for reference. **Do not build from them or wire from them.** Their problems are explained in [docs/reviews/2026-10-firmware-code-tour.md](../docs/reviews/2026-10-firmware-code-tour.md).

| File | What it is | Warning |
| --- | --- | --- |
| `v2.1/sketch_jun22b.ino` | v2.1 single-file sketch | Does not compile. Radio failsafe cannot detect link loss; channels off by one |
| `v2.2/sketch_jun22b_OOP.ino` | v2.2 OOP sketch | Compiles, but has the same safety bugs. Its motor class lives on in `src/drive/BTS7960Motor` |
| `v2.2/platformio.ini` | Old build config | No `src/` folder, unpinned libraries, unused MCP23017 libraries |
| `docs-v2/README.md` | Old project readme | Half 12 V-era: wrong divider, thresholds and pin names |
| `docs-v2/WIRING_GUIDE.md` | Old wiring guide | Entirely 12 V-era |
| `docs-v2/BTS7960_WIRING_GUIDE.md` | BTS7960 wiring | Allows tying EN to VCC, which removes the only hardware stop |
| `docs-v2/GPIO_PIN_ALLOCATION.md` | v2.1 pin map | Pins 34, 35 and 39 each shared by two signals; RL_RPWM on strapping pin 12 |
| `docs-v2/THERMAL_MONITORING_OPTIONS.md` | Thermal "FAULT" input | The BTS7960 has no FAULT pin. Faults show as IS pulled high |
| `docs-v2/BTS7960_TELEMETRY_GUIDE.md`, `TELEMETRY_UPDATE_SUMMARY.md` | Current-sense telemetry | Eight channels on pins the code abandoned; scaling about 100 times wrong |
| `docs-v2/HARDWARE_UPDATES_v2.1.md`, `QUICK_START_GUIDE.md` | v2.1 hardware notes | Match v2.1, not v3 |
| `docs-v2/OOP_REFACTORING_GUIDE.md` | v2.2 refactor notes | Claims v2.1 compiles and is "production ready". Neither is true |
| `docs-v2/USER_MANUAL.md` | v2.1 user manual | About 93 % whitespace padding (649 KB) |
| `docs-v2/CHANGELOG.md` | History to v2.0 | Continued in the top-level [CHANGELOG.md](../CHANGELOG.md) |
| `hardware_reference/Component_Descriptions_and_Purpose.pdf` | 24 V component list | Roughly right; fed [docs/hardware/BOM.md](../docs/hardware/BOM.md) |
| `hardware_reference/ESP32_ELRS_Receiver_Connection_Guide.pdf` | ELRS receiver guide | Wrong: uses SBUS, and suggests flashing ELRS onto the ESP32 |
| `hardware_reference/Untitled Document.pdf` | A PDF render of USER_MANUAL.md | Same content as the manual |
| `README_github_2024.md` | The GitHub readme before v3 | Superseded by the top-level README |

`unrelated/` (local only, ignored by git) holds a ROM-download script that was misfiled in this project.
