# EdgeTX files

`sdcard/` mirrors the radio's SD card. `radio/install_radio.py` copies these files into place ([../README.md](../README.md)); by hand, copy each file to the same path on the card.

| File | What it is |
| --- | --- |
| `MODELS/model42.yml` | The `RideOn` model ([../MODEL_SETUP.md](../MODEL_SETUP.md)). EdgeTX's YAML reader has no comments, so its notes live in MODEL_SETUP.md |
| `SCRIPTS/TELEMETRY/rideon.lua` | Telemetry screen and alarms |
| `SCRIPTS/TOOLS/RideOnRF.lua` | "RideOn ELRS setup": sets the ELRS module, teaches Model Match, writes `/RideOn-radio.txt` with the exact versions ([../ELRS_SETUP.md](../ELRS_SETUP.md)) |

Both scripts and the model are checked in CI (`tools/check_radio_files.py`, `tools/test_radio_scripts.py`).

Later: a tools script that changes the car's limits and calibrates steering from the radio over CRSF, so no laptop is needed. It comes after the safety work.
