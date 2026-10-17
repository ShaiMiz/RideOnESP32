#!/usr/bin/env python3
"""Check that include/pins.h, docs/hardware/PINOUT.md and docs/hardware/WIRING.md agree.

pins.h is the source of truth. This fails (exit 1) when:
  - a pin constant is missing from pins::MAP, or a MAP entry's name doesn't match its constant
  - PINOUT.md's table is missing a pin, lists an extra one, or gives a different GPIO
  - WIRING.md's ESP32 table doesn't list exactly the GPIOs pins.h uses

Usage: python3 tools/check_pin_docs.py   (from the repo root)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors = []


def pins_h():
    text = (ROOT / "include/pins.h").read_text()
    body = text[text.index("namespace pins {"):text.index("}  // namespace pins")]
    consts = {m[1]: int(m[2]) for m in re.finditer(r"constexpr uint8_t (\w+) = (\d+);", body)}
    entries = re.findall(r'\{(\w+), "(\w+)", Use::\w+\}', body)
    for const, name in entries:
        if const != name:
            errors.append(f"pins.h: MAP entry {{{const}, \"{name}\"}} names the wrong pin")
    in_map = {c for c, _ in entries}
    for c in consts:
        if c not in in_map:
            errors.append(f"pins.h: {c} is not in pins::MAP, so the build rules don't check it")
    return consts


def pinout_md():
    rows = {}
    for line in (ROOT / "docs/hardware/PINOUT.md").read_text().splitlines():
        m = re.match(r"\|\s*[LR]\d+\s*\|\s*(\d+)\s*\|\s*`(\w+)`\s*\|", line)
        if m:
            if m[2] in rows:
                errors.append(f"PINOUT.md: {m[2]} is listed twice")
            rows[m[2]] = int(m[1])
    return rows


def wiring_md():
    text = (ROOT / "docs/hardware/WIRING.md").read_text()
    section = text[text.index("## ESP32"):text.index("## I2C bus")]
    gpios = []
    for line in section.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 2 or not re.match(r"^\d", cells[0]) or cells[1] == "Nothing":
            continue
        gpios += [int(n) for n in re.findall(r"\b(\d+)\b(?!\s*V)", cells[0])]
    return gpios


consts = pins_h()
pinout = pinout_md()
for name, gpio in consts.items():
    if name not in pinout:
        errors.append(f"PINOUT.md: no row for {name} (GPIO{gpio})")
    elif pinout[name] != gpio:
        errors.append(f"PINOUT.md: {name} is GPIO{pinout[name]}, pins.h says GPIO{gpio}")
for name in pinout:
    if name not in consts:
        errors.append(f"PINOUT.md: {name} is not in pins.h")

wiring = wiring_md()
used = sorted(consts.values())
for g in sorted(set(wiring)):
    if wiring.count(g) > 1:
        errors.append(f"WIRING.md: GPIO{g} is listed more than once")
if sorted(set(wiring)) != used:
    missing = sorted(set(used) - set(wiring))
    extra = sorted(set(wiring) - set(used))
    if missing:
        errors.append(f"WIRING.md: missing GPIO {missing}")
    if extra:
        errors.append(f"WIRING.md: lists GPIO {extra}, which pins.h doesn't use")

if errors:
    print("Pin docs check FAILED:")
    for e in errors:
        print("  -", e)
    sys.exit(1)
print(f"Pin docs check passed: {len(consts)} pins agree across pins.h, PINOUT.md and WIRING.md.")
