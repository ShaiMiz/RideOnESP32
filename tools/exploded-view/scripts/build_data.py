#!/usr/bin/env python3
"""Build src/data/project.json for the exploded view from the project's own records.

Sources, all generated or kept elsewhere in the repo (never edit project.json by hand):
  docs/hardware/kicad/BOM.csv        parts, quantities, status, notes
  docs/hardware/kicad/WIRE_LIST.csv  every wire: from, to, net, AWG, colour, fuse, route
  docs/hardware/kicad/CONNECTORS.md  box-edge plugs J1-J12 and their status
  docs/hardware/PINOUT.md            ESP32 header pin -> GPIO and signal
  docs/hardware/CAR.md               car facts (size, weight)
  docs/hardware/kicad/gen/design.py  variant B drive parts and their nets (#32)

Where each part sits in 3D and what shape it gets is in src/data/layout.js; a BOM part
with no layout entry still shows up, on a tray beside the car, so nothing goes missing.

  python3 scripts/build_data.py                 # read the working tree
  python3 scripts/build_data.py --git-ref REF   # read another branch, e.g. an open PR
"""
import argparse
import csv
import io
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
APP = HERE.parent
ROOT = APP.parent.parent
OUT = APP / "src" / "data" / "project.json"

KICAD = "docs/hardware/kicad"
SOURCES = {
    "bom": f"{KICAD}/BOM.csv",
    "wires": f"{KICAD}/WIRE_LIST.csv",
    "connectors": f"{KICAD}/CONNECTORS.md",
    "pinout": "docs/hardware/PINOUT.md",
    "car": "docs/hardware/CAR.md",
}

REF_RE = re.compile(r"^[A-Z]+[0-9]+$")
END_RE = re.compile(r"^([A-Z]+[0-9]*)[.:](.+)$")
ISSUE_RE = re.compile(r"#(\d+)")


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True, text=True).stdout


def reader(ref):
    if ref is None:
        return lambda p: (ROOT / p).read_text(encoding="utf-8")
    return lambda p: git("show", f"{ref}:{p}")


def pick(row, *names):
    """First matching column, case-insensitive, so a reshuffled BOM still parses."""
    low = {k.strip().lower(): k for k in row}
    for n in names:
        if n in low:
            return (row[low[n]] or "").strip()
    return ""


def issues(*texts):
    found = []
    for t in texts:
        for n in ISSUE_RE.findall(t or ""):
            if n not in found:
                found.append(n)
    return found


def parse_bom(text):
    rows, parts = [], {}
    for i, row in enumerate(csv.DictReader(io.StringIO(text))):
        refs_raw = pick(row, "refs", "ref", "reference", "references", "designator", "designators")
        refs = [r for r in re.split(r"[\s,]+", refs_raw) if REF_RE.match(r)]
        line = pick(row, "#", "line", "no", "no.", "item", "line #", "bom line") or str(i + 1)
        entry = {
            "line": line,
            "refs": refs,
            "qty": pick(row, "qty", "quantity"),
            "part": pick(row, "description", "part", "item name"),
            "model": pick(row, "manufacturer / part number", "manufacturer", "mpn", "part number"),
            "category": pick(row, "category"),
            "where": pick(row, "where", "location"),
            "status": pick(row, "status"),
            "note": pick(row, "notes", "note"),
            "fields": {k.strip(): (v or "").strip() for k, v in row.items() if k and (v or "").strip()},
        }
        tool = refs_raw.lower().startswith("tool") or entry["category"].lower() in ("tool", "tools")
        entry["kind"] = "tool" if tool else ("part" if refs else "material")
        entry["issues"] = issues(entry["part"], entry["model"], entry["note"], entry["status"])
        rows.append(entry)
        for r in refs:
            parts[r] = {"ref": r, "bomLine": line, "bomIndex": i}
    return rows, parts


def parse_wires(text):
    wires = []
    for row in csv.DictReader(io.StringIO(text)):
        def end(s):
            m = END_RE.match(s.strip())
            return {"ref": m.group(1), "pin": m.group(2)} if m else {"ref": s.strip(), "pin": ""}
        wires.append({
            "id": pick(row, "wire"),
            "from": end(pick(row, "from")),
            "to": end(pick(row, "to")),
            "net": pick(row, "net"),
            "awg": pick(row, "awg"),
            "colour": pick(row, "colour", "color"),
            "protectedBy": pick(row, "protected by"),
            "route": pick(row, "route"),
            "note": pick(row, "note"),
        })
    return wires


def md_tables(text):
    """Every Markdown table as a list of header->cell dicts."""
    tables, cur, head = [], None, None
    for line in text.splitlines():
        if not line.startswith("|"):
            cur = head = None
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if head is None:
            head, cur = cells, []
            tables.append(cur)
        elif set("".join(cells)) <= set("-: "):
            continue
        else:
            cur.append(dict(zip(head, cells)))
    return tables


def strip_md(s):
    return re.sub(r"[`*]", "", re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", s or "")).strip()


def parse_pinout(text):
    pins = {}
    for t in md_tables(text):
        for r in t:
            if "Header" in r and "GPIO" in r:
                pins[r["Header"]] = {"gpio": r["GPIO"], "signal": strip_md(r.get("Signal")), "dir": r.get("Dir", ""),
                                     "connects": strip_md(r.get("Connects to")), "why": strip_md(r.get("Why this pin, and what to watch"))}
    return pins


def parse_connectors(text):
    for t in md_tables(text):
        if t and "Faces" in t[0]:
            return {r["Ref"]: {"name": r["Name"], "pins": r["Pins"], "faces": r["Faces"], "part": r["Part"],
                               "status": r["Status"], "tbd": "TBD" in r["Faces"] or r["Status"] == "TBD"} for r in t}
    return {}


def parse_car(text):
    for t in md_tables(text):
        if t and "Fact" in t[0]:
            return {strip_md(r["Fact"]): strip_md(r["Value"]) for r in t}
    return {}


def design_data(ref):
    """Pins and nets of every schematic part, and the variant B drive parts (#32), straight from design.py."""
    with tempfile.TemporaryDirectory() as tmp:
        if ref is None:
            gen = ROOT / KICAD / "gen"
        else:
            arch = subprocess.run(["git", "-C", str(ROOT), "archive", ref, f"{KICAD}/gen", "include"],
                                  check=True, capture_output=True).stdout
            subprocess.run(["tar", "-x", "-C", tmp], input=arch, check=True)
            gen = Path(tmp) / KICAD / "gen"
        sys.path.insert(0, str(gen))
        try:
            import design  # noqa: E402
        finally:
            sys.path.pop(0)

        def part(p, sheet):
            return {"ref": p.ref, "value": p.value, "part": p.part, "source": p.source, "where": p.loc,
                    "note": p.note, "issue": p.issue, "sheet": sheet, "pins": p.pins}

        nets = {}
        for sh in design.SHEETS:
            for p in sh.get("parts", []):
                if getattr(p, "in_bom", True) and REF_RE.match(p.ref):
                    nets[p.ref] = part(p, sh.get("name", ""))
        b = [part(p, design.VARIANT_B_SHEET["name"]) for p in getattr(design, "VARIANT_B", [])
             if getattr(p, "in_bom", True) and REF_RE.match(p.ref)]
        return nets, b


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--git-ref", help="read the sources from this git ref instead of the working tree")
    ap.add_argument("--check", action="store_true", help="fail if project.json is out of date")
    a = ap.parse_args()
    read = reader(a.git_ref)

    bom, part_index = parse_bom(read(SOURCES["bom"]))
    wires = parse_wires(read(SOURCES["wires"]))
    data = {
        "meta": {
            "sources": SOURCES,
            "gitRef": a.git_ref or "working tree",
            "issuesUrl": "https://github.com/ShaiMiz/RideOnESP32/issues/",
        },
        "car": parse_car(read(SOURCES["car"])),
        "bom": bom,
        "partIndex": part_index,
        "wires": wires,
        "connectors": parse_connectors(read(SOURCES["connectors"])),
        "esp32Pins": parse_pinout(read(SOURCES["pinout"])),
    }
    data["design"], data["variantB"] = design_data(a.git_ref)
    text = json.dumps(data, ensure_ascii=False, indent=1) + "\n"
    if a.check:
        if not OUT.exists() or OUT.read_text(encoding="utf-8") != text:
            sys.exit(f"{OUT.relative_to(ROOT)} is stale: run `npm run data` in tools/exploded-view")
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(bom)} BOM lines, {len(part_index)} refs, {len(wires)} wires, "
          f"{len(data['variantB'])} variant B parts, from {data['meta']['gitRef']}")


if __name__ == "__main__":
    main()
