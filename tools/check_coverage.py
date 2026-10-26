#!/usr/bin/env python3
"""Coverage floors for lib/rideon_core, from the gcov data that
tools/run_native_tests.sh leaves behind (docs/VERIFICATION.md).

    python3 tools/check_coverage.py [build-dir]

Fails when
  - line coverage of the core falls under LINE_FLOOR, or branch coverage
    under BRANCH_FLOOR (both set just under what the tests reach today, so
    coverage can only go up), or
  - any line of a safety file is never run by a test. These files decide
    whether the car may move; every line of them must be exercised.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINE_FLOOR = 92.0
BRANCH_FLOOR = 75.0
SAFETY_FILES = [
    "ArmingCheck.cpp",
    "BatteryCheck.cpp",
    "BoxTempCheck.cpp",
    "ChannelMap.h",
    "ContactorCheck.cpp",
    "Controller.cpp",
    "Faults.h",
    "LinkMonitor.cpp",
    "ModeMachine.cpp",
    "Ramp.cpp",
    "SafetyRules.cpp",
]


def main() -> int:
    build = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / ".pio" / "verify"
    out = subprocess.run(
        ["gcovr", "--root", str(ROOT), "--filter", "lib/rideon_core/src/", "--object-directory", str(build),
         "--exclude-throw-branches", "--exclude-unreachable-branches", "--json-summary", "-", str(build)],
        check=True, capture_output=True, text=True).stdout
    summary = json.loads(out)
    errors = []
    line, branch = summary["line_percent"], summary["branch_percent"]
    print(f"rideon_core coverage: lines {line:.1f}% (floor {LINE_FLOOR}), branches {branch:.1f}% (floor {BRANCH_FLOOR})")
    if line < LINE_FLOOR:
        errors.append(f"line coverage {line:.1f}% is under the floor {LINE_FLOOR}%")
    if branch < BRANCH_FLOOR:
        errors.append(f"branch coverage {branch:.1f}% is under the floor {BRANCH_FLOOR}%")
    files = {Path(f["filename"]).name: f for f in summary["files"]}
    for name in SAFETY_FILES:
        f = files.get(name)
        if f is None:
            errors.append(f"{name}: no coverage data (not compiled into any test?)")
        elif f["line_covered"] < f["line_total"]:
            errors.append(f"{name}: {f['line_total'] - f['line_covered']} line(s) never run by a test")
    for e in errors:
        print("ERROR:", e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
