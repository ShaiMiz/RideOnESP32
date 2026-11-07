#!/usr/bin/env python3
"""Check the EdgeTX radio files against the firmware.

- radio/edgetx/sdcard/MODELS/model42.yml sends each control on the channel that
  lib/rideon_core/src/ChannelMap.h reads, with the right polarity, trims off,
  start-up switch warnings on, the internal ELRS module and the receiver number 42.
- docs/firmware/CHANNEL_MAP.md names the same switches.
- The Lua alarm thresholds match include/config.h.
- The file has no YAML comments or tabs (EdgeTX's own parser does not understand them).

Needs PyYAML. Run from the repo root: python3 tools/check_radio_files.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
MODEL = ROOT / "radio/edgetx/sdcard/MODELS/model42.yml"
TELE_LUA = ROOT / "radio/edgetx/sdcard/SCRIPTS/TELEMETRY/rideon.lua"
TOOL_LUA = ROOT / "radio/edgetx/sdcard/SCRIPTS/TOOLS/RideOnRF.lua"
CHANNEL_MAP_H = ROOT / "lib/rideon_core/src/ChannelMap.h"
CHANNEL_MAP_MD = ROOT / "docs/firmware/CHANNEL_MAP.md"
CONFIG_H = ROOT / "include/config.h"
RADIO_README = ROOT / "radio/README.md"

RECEIVER_NUMBER = 42

# ChannelMap.h constant -> (EdgeTX source, weight). Positive weight on a switch: up = -100 %.
EXPECTED = {
    "STEERING": ("Ail", 100),   # right stick sideways in mode 2
    "THROTTLE": ("Ele", 100),   # right stick up/down in mode 2, forward = +100 %
    "KILL": ("SC", 100),        # up = 988 us = KILL, down = 2012 us = RUN
    "MODE": ("SA", 100),        # up = Locked, middle = Kid, down = Parent
    "SPEED_CAP": ("SB", 100),   # up = 30 %, middle = 60 %, down = 100 %
}
SWITCH_DOC = {"KILL": "SC", "MODE": "SA", "SPEED_CAP": "SB"}

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def channel_numbers() -> dict[str, int]:
    text = CHANNEL_MAP_H.read_text()
    return {m.group(1): int(m.group(2))
            for m in re.finditer(r"constexpr uint8_t (\w+) = (\d+);", text) if m.group(1) != "COUNT"}


def config_value(name: str) -> float:
    m = re.search(rf"\b{name}\s*=\s*([\d.]+)f?", CONFIG_H.read_text())
    if not m:
        raise SystemExit(f"{name} not found in include/config.h")
    return float(m.group(1))


def lua_value(path: Path, name: str) -> float | None:
    m = re.search(rf"^local {name}\s*=\s*([\d.]+)", path.read_text(), re.MULTILINE)
    return float(m.group(1)) if m else None


def main() -> int:
    raw = MODEL.read_text()
    for n, line in enumerate(raw.splitlines(), 1):
        if line.lstrip().startswith("#"):
            err(f"model42.yml:{n}: comment line; EdgeTX's YAML reader has no comments")
        if "\t" in line:
            err(f"model42.yml:{n}: tab; use spaces")
    model = yaml.safe_load(raw)

    if model["header"]["name"] != "RideOn":
        err("model name must be RideOn (install_radio.py looks for it)")
    if model["header"]["modelId"][0]["val"] != RECEIVER_NUMBER:
        err(f"internal module receiver number must be {RECEIVER_NUMBER}")
    if f"{RECEIVER_NUMBER}" not in RADIO_README.read_text():
        err(f"radio/README.md should name receiver number {RECEIVER_NUMBER}")

    mod = model["moduleData"][0]
    if mod["type"] != "TYPE_CROSSFIRE":
        err("internal module must be TYPE_CROSSFIRE (ELRS)")

    mixes: dict[int, list[dict]] = {}
    for mix in model["mixData"]:
        mixes.setdefault(mix["destCh"] + 1, []).append(mix)

    chans = channel_numbers()
    for const, (src, weight) in EXPECTED.items():
        ch = chans[const]
        lines = mixes.get(ch, [])
        if len(lines) != 1 or lines[0]["srcRaw"] != src or lines[0]["weight"] != weight:
            err(f"CH{ch} ({const}) must be one mix line from {src} at weight {weight}, got {lines}")
        elif lines[0].get("swtch") not in (None, "NONE"):
            err(f"CH{ch} ({const}) must not depend on a switch")
    for const in ("STEERING", "THROTTLE"):
        for mix in mixes.get(chans[const], []):
            if mix.get("carryTrim") != 1:
                err(f"CH{chans[const]} ({const}) must not carry trims (carryTrim: 1)")

    # Horn: -100 % always, replaced by +100 % while either T1 trim button (under the left stick)
    # is held. With the trims off, EdgeTX lets the trim buttons act as push-button switches.
    horn = mixes.get(chans["HORN"], [])
    ok = (len(horn) == 3 and horn[0]["srcRaw"] == "MAX" and horn[0]["weight"] == -100
          and horn[0].get("swtch") in (None, "NONE")
          and all(h["srcRaw"] == "MAX" and h["weight"] == 100 and h["mltpx"] == "REPL" for h in horn[1:])
          and sorted(h.get("swtch") for h in horn[1:]) == ["TrimRudLeft", "TrimRudRight"])
    if not ok:
        err("CH8 horn must be MAX -100 %, replaced by +100 % while trim button T1- or T1+ is held "
            "(swtch TrimRudLeft / TrimRudRight)")
    if model.get("logicalSw"):
        err("no logical switches are expected; the horn uses the T1 trim buttons")

    warn = model.get("switchWarningState", "")
    for sw in ("Au", "Bu", "Cu"):
        if sw not in warn:
            err(f"switchWarningState must include {sw} (start-up check)")

    trims = model["flightModeData"][0]["trim"]
    if any(trims.get(i, {}).get("mode") != 31 for i in range(4)):
        err("all four trims must be off in flight mode 0 (mode: 31)")

    labels = {s["label"] for s in model["telemetrySensors"].values()}
    for need in ("RxBt", "Curr", "Capa", "Bat%", "FM", "RQly", "1RSS", "TPWR"):
        if need not in labels:
            err(f"telemetry sensor {need} missing")

    script = model["screens"][0]["u"]["script"]["file"]
    if len(script) > 6 or not (TELE_LUA.parent / f"{script}.lua").is_file():
        err(f"telemetry screen script {script!r} must exist and be at most 6 characters")

    if lua_value(TELE_LUA, "BATT_WARN_V") != config_value("BATTERY_LOW_WARN_V"):
        err("rideon.lua BATT_WARN_V must equal BATTERY_LOW_WARN_V in config.h")
    if lua_value(TELE_LUA, "LQ_WARN") != config_value("LQ_DEGRADED_BELOW"):
        err("rideon.lua LQ_WARN must equal LQ_DEGRADED_BELOW in config.h")

    doc = CHANNEL_MAP_MD.read_text()
    for const, sw in SWITCH_DOC.items():
        row = next((l for l in doc.splitlines() if f"`ch::{const}`" in l), "")
        if sw not in row:
            err(f"CHANNEL_MAP.md row for ch::{const} must name switch {sw}")

    if not TOOL_LUA.read_text().startswith("-- TNS|"):
        err("RideOnRF.lua must start with the EdgeTX tool name line")

    for e in errors:
        print(f"ERROR: {e}")
    if not errors:
        print("radio files OK: model, channel map, thresholds and scripts agree")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
