#!/usr/bin/env python3
"""Checks KiCad's own netlist export against design.py and include/pins.h.

The layout draws a stub and a label on every pin, so a misplaced stub would silently leave a pin off
its net. This reads the netlist KiCad computes from the drawn sheets and confirms every pin landed on
the net design.py intended, and that each ESP32 GPIO carries the pins.h signal of that number.

Usage: check_netlist.py NETLIST.net   (KiCad s-expression netlist; check.sh makes it)
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import design as D  # noqa: E402
import generate as G  # noqa: E402


def kicad_nets(path):
    text = Path(path).read_text()
    nets = {}
    for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]*)"\)(.*?)\)\)\s*(?=\(net |\)\s*\)\s*$)', text, re.S):
        name = m[1].rsplit("/", 1)[-1]
        if name.startswith("unconnected-"):
            continue
        nets[name] = {(r, p) for r, p in re.findall(r'\(ref "([^"]+)"\) \(pin "([^"]+)"\)', m[2])}
    return nets


def main():
    got = kicad_nets(sys.argv[1])
    syms = G.build_symbols()
    gpio = G.pins_h()
    esp = next(p for p in D.controller if p.ref == "U20")
    G.wire_esp32(esp, syms[esp.sym], gpio)
    want = {}
    for sd in D.SHEETS:
        for p in sd["parts"]:
            for key, net in p.pins.items():
                if net and not p.ref.startswith("#"):  # power flags are not in KiCad's netlist
                    want.setdefault(net, set()).add((p.ref, syms[p.sym].pin(key).number))
    errors = []
    for net in sorted(set(want) | set(got)):
        if want.get(net) != got.get(net):
            errors.append(f"{net}: design {sorted(want.get(net, []))} vs KiCad {sorted(got.get(net, []))}")
    # ESP32 header pins against pins.h, straight from KiCad's view
    sym = syms[esp.sym]
    for p in sym.pins:
        m = re.match(r"(?:TX0/|RX0/)?GPIO(\d+)", p.name)
        if m and int(m[1]) in gpio:
            name = gpio[int(m[1])]
            if ("U20", p.number) not in got.get(name, set()):
                errors.append(f"U20.{p.number} (GPIO{m[1]}) is not on net {name} from pins.h")
    if errors:
        print("netlist check FAILED:\n  " + "\n  ".join(errors))
        sys.exit(1)
    print(f"netlist check OK: {len(got)} nets match design.py; {len(gpio)} ESP32 pins match pins.h")


if __name__ == "__main__":
    main()
