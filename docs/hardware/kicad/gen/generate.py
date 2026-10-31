#!/usr/bin/env python3
"""Generates the RideOnESP32 KiCad schematic project and its tables from design.py.

Usage (from the repo root):
    python3 docs/hardware/kicad/gen/generate.py

Writes docs/hardware/kicad/*.kicad_{pro,sch,sym}, variant_b/, and the tables
(NETS.md, CONNECTORS.md, WIRE_LIST.md/.csv, BOM.md/.csv). Edit design.py or wiring.py, never the outputs.
Then run docs/hardware/kicad/check.sh for KiCad's ERC and the PDF/SVG/netlist exports.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent
ROOT = OUT.parents[2]
sys.path.insert(0, str(HERE))

import design as D  # noqa: E402
import kicad as K  # noqa: E402
import overview  # noqa: E402
import tables  # noqa: E402

PROJECT = "rideonesp32"
DATE = "2026-10-24"
REV = "0.3"


def pins_h():
    """GPIO -> signal name from include/pins.h (namespace pins only)."""
    text = (ROOT / "include/pins.h").read_text()
    body = text[text.index("namespace pins {"):text.index("}  // namespace pins")]
    return {int(m[2]): m[1] for m in re.finditer(r"constexpr uint8_t (\w+) = (\d+);", body)}


def wire_esp32(part, sym, gpio_names):
    for p in sym.pins:
        m = re.match(r"(?:TX0/|RX0/)?GPIO(\d+)", p.name)
        if p.name in ("GND",):
            part.pins[p.number] = "GND"
        elif p.name == "VIN":
            part.pins[p.number] = "5V_LOGIC"
        elif p.name.startswith("TX0"):
            part.pins[p.number] = "LOG_TX"  # the USB serial log, also heard by the Bluetooth module U33
        elif p.name in ("EN", "3V3") or p.name.startswith("RX0"):
            part.pins[p.number] = None
        elif m:
            part.pins[p.number] = gpio_names.get(int(m[1]))
            if part.pins[p.number] is None:
                raise SystemExit(f"GPIO{m[1]} is on the header but not in pins.h; mark it unused on purpose")


def build_symbols():
    syms = K.load_stock(OUT / "stocklib/stock_symbols.kicad_sexp")
    for name, m in D.MODULES.items():
        syms[D.M(name)] = K.module_symbol(name, m["left"], m["right"], m["ref"], descr=m["descr"])
    return syms


def auto_rot(part, sym):
    if part.rot is not None:
        return part.rot
    outs = [K.rot_pin(p, 0)[1] for p in sym.pins]
    if outs and all(o[0] == 0 for o in outs) and len(sym.pins) == 2:
        return 90
    return 0


def resolve_columns(sheet, parts):
    by_ref = {p.ref: p for p in parts}
    used = set()
    s = K.Sheet(sheet["name"], sheet["file"] + ".kicad_sch", sheet["name"], None, sheet["comments"])
    for title, refs in sheet["columns"]:
        items = []
        for r in refs:
            if r.startswith("NOTE:"):
                n = D.NOTES[r[5:]]
                items.append(K.Note(n[0], n[1]))
            else:
                items.append(by_ref[r])
                used.add(r)
        s.column(*items, title=title or None)
    missing = set(by_ref) - used
    if missing:
        raise SystemExit(f"{sheet['file']}: parts not placed on the sheet: {sorted(missing)}")
    return s


def nets_of(parts, syms):
    nets = defaultdict(list)
    for p in parts:
        sym = syms[p.sym]
        for key, net in p.pins.items():
            if net is not None:
                nets[net].append((p.ref, sym.pin(key).number, sym.pin(key).name))
    return nets


def check_nets(nets, allow_single=()):
    bad = [n for n, pins in nets.items() if len({(r, num) for r, num, _ in pins}) < 2 and n not in allow_single]
    if bad:
        raise SystemExit(f"nets with only one pin (typo?): {bad}")


def build_project(name, sheets_def, syms, root_notes, allow_single=()):
    """Writes one KiCad project: a root sheet holding one sheet symbol per entry of sheets_def."""
    outdir = OUT if name == PROJECT else OUT / name
    outdir.mkdir(exist_ok=True)
    root_uuid = K.uid(name, "root")
    w = K.Writer(name, syms, root_uuid, DATE, REV)
    all_parts = []
    sheet_parts = {}
    for sd in sheets_def:
        for p in sd["parts"]:
            p.sheet = sd["file"]
            p.rot = auto_rot(p, syms[p.sym])
        all_parts += sd["parts"]
        sheet_parts[sd["file"]] = sd["parts"]
    refs = [p.ref for p in all_parts]
    dup = {r for r in refs if refs.count(r) > 1}
    if dup:
        raise SystemExit(f"duplicate references: {dup}")
    nets = nets_of(all_parts, syms)
    check_nets(nets, allow_single)
    # a net is global when it appears on more than one sheet
    where = defaultdict(set)
    for p in all_parts:
        for net in p.pins.values():
            if net:
                where[net].add(p.sheet)
    w.global_nets = {n for n, s in where.items() if len(s) > 1 or n in allow_single}

    sheet_objs = []
    for i, sd in enumerate(sheets_def):
        su = K.uid(name, "sheet", sd["file"])
        path = f"/{root_uuid}/{su}"
        if sd.get("drawing"):  # a drawing-only page (the system overview): graphics, no parts or nets
            s = K.Sheet(sd["name"], f"{sd['file']}.kicad_sch", sd["name"], str(i + 2))
            s.paper = sd["paper"]
            (outdir / s.filename).write_text(w.sheet_file(s, K.uid(name, "file", sd["file"]), path, set(), extra_items=sd["drawing"]()))
            sheet_objs.append((s, su))
            continue
        s = resolve_columns(sd, sd["parts"])
        s.page = str(i + 2)
        placed = w.layout(s)
        libs = {p.sym for p in sd["parts"]}
        (outdir / s.filename).write_text(w.sheet_file(s, K.uid(name, "file", sd["file"]), path, libs, placed=placed))
        sheet_objs.append((s, su))

    # root sheet: one sheet symbol per page plus the index notes
    root = K.Sheet("Root", f"{name}.kicad_sch", root_notes["title"], "1", root_notes["comments"])
    root.paper = ("A3", 420.0, 297.0)
    items = []
    x0, y0 = 20.32, 30.48
    for i, (s, su) in enumerate(sheet_objs):
        col, row = i % 4, i // 4
        x, y = x0 + col * 96.52, y0 + row * 33.02
        items.append(
            f"(sheet (at {K.fmt(x)} {K.fmt(y)}) (size 81.28 20.32) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)\n"
            f"  (stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0.0000)) (uuid {K.q(su)})\n"
            f"  (property \"Sheetname\" {K.q(f'{s.page} {s.name}')} (at {K.fmt(x)} {K.fmt(y - 0.7)} 0) {K.effects(justify='left bottom')})\n"
            f"  (property \"Sheetfile\" {K.q(s.filename)} (at {K.fmt(x)} {K.fmt(y + 21)} 0) {K.effects(justify='left top')})\n"
            f"  (instances (project {K.q(name)} (path {K.q('/' + root_uuid)} (page {K.q(s.page)}))))\n)")
        blurb = sheets_def[i].get("blurb")
        if blurb:  # what the sheet holds, inside its box; double-click the box to open the sheet
            items.append(f"(text {K.q(blurb)} (exclude_from_sim no) (at {K.fmt(x + 2.54)} {K.fmt(y + 2.54)} 0) "
                         f"{K.effects(justify='left top')} (uuid {K.q(K.uid(name, 'blurb', s.filename))}))")
    ny = y0 + ((len(sheet_objs) + 3) // 4) * 33.02 + 4
    nx = x0
    for title, lines, width in root_notes["notes"]:
        note = K.Note(title, lines, width)
        if ny + note.height() > 297 - 45:
            ny = y0 + ((len(sheet_objs) + 3) // 4) * 33.02 + 4
            nx += 135
        items += w.emit_note(note, nx, ny, K.uid(name, "rootnote", title))
        ny += note.height() + 6
    (outdir / root.filename).write_text(w.sheet_file(root, root_uuid, "/" + root_uuid, set(), extra_items=items))
    pro = K.project_file(name, [(root_uuid, "Root")] + [(su, s.name) for s, su in sheet_objs])
    if outdir != OUT:
        # variant B is a delta sheet: its flag labels continue on the main project (the ESP32 drives its
        # inputs there), so dangling labels and undriven inputs are expected here and checked in the main ERC
        pro = pro.replace('"single_global_label": "ignore"', '"single_global_label": "ignore", "global_label_dangling": "ignore", "pin_not_driven": "ignore"')
        (outdir / "sym-lib-table").write_text(
            "(sym_lib_table\n  (version 7)\n"
            f"  (lib (name \"{PROJECT}\")(type \"KiCad\")(uri \"${{KIPRJMOD}}/../{PROJECT}.kicad_sym\")(options \"\")(descr \"RideOnESP32 module symbols\"))\n)\n")
    (outdir / f"{name}.kicad_pro").write_text(pro)
    return all_parts, nets, w.global_nets


def write_symbol_lib(syms):
    blocks = []
    for lib_id in sorted(syms):
        if lib_id.startswith(D.M("")):
            t = syms[lib_id].text.replace(K.q(lib_id), K.q(lib_id.split(":", 1)[1]), 1)
            blocks.append(t)
    (OUT / f"{PROJECT}.kicad_sym").write_text(
        "(kicad_symbol_lib (version 20241209) (generator \"rideonesp32_gen\") (generator_version \"9.0\")\n"
        + "\n".join(blocks) + "\n)\n")
    (OUT / "sym-lib-table").write_text(
        "(sym_lib_table\n  (version 7)\n"
        f"  (lib (name \"{PROJECT}\")(type \"KiCad\")(uri \"${{KIPRJMOD}}/{PROJECT}.kicad_sym\")(options \"\")(descr \"RideOnESP32 module symbols\"))\n)\n")


ROOT_NOTES = dict(
    title="RideOnESP32 v3 wiring schematic (index)",
    comments=["Draft v0.3: drawn before the stock plugs, motor and battery labels and box size are known (#13-#18).",
              "Do not build the car-facing side from it until those TBDs are closed.",
              "Generated by gen/generate.py from gen/design.py. Edit the script, not these files."],
    notes=[
        ("How to read it", [
            "Every pin has a stub and a net label: the label names the net, so",
            "each pin says where it goes. Labels with a flag shape are global",
            "(used on more than one sheet); plain text labels stay on one sheet.",
            "Each box above is a sheet: double-click it in KiCad to open it",
            "(or page down in the PDF). Location field: box, A1 (the interface board),",
            "car, dash. Source field: owned, ordered 24 Oct, stock car, to buy, TBD.",
            "Tables: NETS.md, CONNECTORS.md, WIRE_LIST.md, BOM.md (same data)."], 125.0),
        ("Three safety layers (POWER_CHAIN.md)", [
            "1. A power chain a person can always cut, no software:",
            "   F1, key SW1 and E-stop S2 (two NC blocks) in the K1 coil.",
            "2. Drivers that default off: DRIVE_EN / STEER_EN pulled down",
            "   at every board; K1 coil MOSFET Q1 pulled down.",
            "3. The ESP32 only adds control on top, and can only open K1."], 125.0),
        ("Open items (TBD)", [
            "#17 stock plug types and pin use: J1-J8 (car-facing plugs)",
            "#33 motor pairing in the harness: J6/J7 splitter or not",
            "#13 battery voltage and chemistry: D2 TVS value, BTS7960 27.5 V limit",
            "#14 motor labels: per-motor current, variant A or B (#32)",
            "#18 box size: variant A (5 boards) or B (2 boards)",
            "#16 pedal type: switch, pot or 5 V hall (divider)",
            "#15 IBT-2 IS resistor: config IS_RESISTOR_OHMS",
            "Key switch rating (SW1 carries ~8 A worst case)",
            "Shifter: signal-only or motor current (only signal can go to U24)",
            "K1 precharge resistor: after the bench test",
            "AS5600 cable length: I2C or analog OUT on U23 A1",
            "Variant B current sensors (#32)"], 125.0),
        ("Revisions", [
            "0.3 2026-10-24  First KiCad version, from draft v0.2 (SVG).",
            "                Net names, connector/pin tables, wire list, BOM",
            "                matched to the 24 Oct order. Changes are listed",
            "                in README.md, 'Changes from draft v0.2'."], 125.0),
    ])


def main():
    syms = build_symbols()
    gpio = pins_h()
    esp = next(p for p in D.controller if p.ref == "U20")
    wire_esp32(esp, syms[esp.sym], gpio)
    overview_sheet = dict(file="overview", name="System wiring overview", paper=("A2", 594.0, 420.0), parts=[],
                          drawing=overview.build,
                          blurb="Every assembly on one page, joined by real wires:\nbattery to motors, the safety chain, rails, ESP32,\nI2C chain. Drawing only; nets are on pages 3-9.")
    parts, nets, global_nets = build_project(PROJECT, [overview_sheet] + D.SHEETS, syms, ROOT_NOTES)
    write_symbol_lib(syms)
    vb_root = dict(title="RideOnESP32 v3, drive variant B (2 x MDD20A)",
                   comments=["Alternative to sheet 6 of the main project; undecided (#32).",
                             "Nets with flag labels connect to the main project's nets of the same name."],
                   notes=[])
    # variant B's ESP32-side and motor-side nets end on the main project (motor + through F11-F14), so they show one pin here
    vb_single = ["FL_RPWM", "FR_RPWM", "RL_RPWM", "RR_RPWM", "FL_LPWM", "FR_LPWM", "RL_LPWM", "RR_LPWM",
                 "FL_DRV_POS", "FR_DRV_POS", "RL_DRV_POS", "RR_DRV_POS", "FL_M_NEG", "FR_M_NEG", "RL_M_NEG", "RR_M_NEG",
                 "IS_FL", "IS_FR", "IS_RL", "IS_RR"]
    vb_parts, vb_nets, _ = build_project("variant_b", [D.VARIANT_B_SHEET], syms, vb_root, allow_single=vb_single)
    tables.write_all(OUT, parts, nets, syms, gpio, vb_parts)
    print(f"{len(parts)} parts, {len(nets)} nets, {len(D.SHEETS) + 1} sheets; variant B {len(vb_parts)} parts")


if __name__ == "__main__":
    main()
