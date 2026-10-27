#!/usr/bin/env python3
"""Traceability check for docs/VERIFICATION.md.

Fails when
  - a requirement row names a test that does not exist or is never run,
  - a requirement has neither a test nor a commissioning gate,
  - a gate names a phase that COMMISSIONING.md does not have,
  - a test, or a mutant in tools/mutation_check.py, names an SR- ID that the
    page does not list,
  - a test function is written but never registered with RUN_TEST,
  - a fault code in Faults.h is never used in test_controller (every fault
    must be driven through the whole tick at least once).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "docs" / "VERIFICATION.md"
COMMISSIONING = ROOT / "docs" / "COMMISSIONING.md"
FAULTS = ROOT / "lib" / "rideon_core" / "src" / "Faults.h"
MUTANTS = ROOT / "tools" / "mutation_check.py"


def tests_by_suite():
    """suite -> (defined test functions, registered test functions)"""
    out = {}
    for f in sorted((ROOT / "test").glob("test_*/*.cpp")):
        text = f.read_text()
        defined = set(re.findall(r"^void (test_\w+)\s*\(", text, re.M))
        run = set(re.findall(r"RUN_TEST\((test_\w+)\)", text))
        d, r = out.setdefault(f.parent.name, (set(), set()))
        d |= defined
        r |= run
    return out


def main() -> int:
    errors = []
    doc = DOC.read_text()
    suites = tests_by_suite()
    phases = set(re.findall(r"^## Phase (\d+)", COMMISSIONING.read_text(), re.M))

    rows = re.findall(r"^\| (SR-\d+) \|([^|]*)\|([^|]*)\|([^|]*)\|\s*$", doc, re.M)
    ids = [r[0] for r in rows]
    if not rows:
        errors.append("no requirement rows found in VERIFICATION.md")
    for dup in sorted({i for i in ids if ids.count(i) > 1}):
        errors.append(f"{dup} is listed more than once")

    for rid, _text, tests_cell, gates_cell in rows:
        refs = re.findall(r"`([\w]+)/(test_\w+)`", tests_cell)
        gates = re.findall(r"Phase (\d+)", gates_cell)
        if not refs and not gates:
            errors.append(f"{rid}: no test and no commissioning gate verifies it")
        for suite, test in refs:
            if suite not in suites:
                errors.append(f"{rid}: no test suite test/{suite}")
                continue
            defined, run = suites[suite]
            if test not in defined:
                errors.append(f"{rid}: {suite}/{test} does not exist")
            elif test not in run:
                errors.append(f"{rid}: {suite}/{test} is never run (no RUN_TEST)")
        for p in gates:
            if p not in phases:
                errors.append(f"{rid}: COMMISSIONING.md has no phase {p}")

    known = set(ids)
    sources = list((ROOT / "test").glob("test_*/*.cpp")) + [MUTANTS]
    for f in sources:
        for rid in sorted(set(re.findall(r"SR-\d+", f.read_text()))):
            if rid not in known:
                errors.append(f"{f.relative_to(ROOT)} names {rid}, which VERIFICATION.md does not list")

    for suite, (defined, run) in sorted(suites.items()):
        for t in sorted(defined - run):
            errors.append(f"{suite}/{t} is written but never run (add RUN_TEST)")

    faults = re.findall(r"^\s*(FAULT_\w+) = 1u << \d+", FAULTS.read_text(), re.M)
    controller = (ROOT / "test" / "test_controller" / "test_main.cpp").read_text()
    for fault in faults:
        if not re.search(rf"\b{fault}\b", controller):
            errors.append(f"{fault} is never exercised in test_controller")

    for e in errors:
        print("ERROR:", e)
    if not errors:
        print(f"VERIFICATION.md: {len(rows)} requirements, every test reference found and run, "
              f"{len(faults)} faults exercised")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
