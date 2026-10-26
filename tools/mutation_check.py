#!/usr/bin/env python3
"""Mutation check: proves the safety tests would notice the safety rules
being broken (docs/VERIFICATION.md).

Each mutant below is one small, plausible mistake in lib/rideon_core: a
dropped condition, a wrong comparison, a missing cap. The script copies the
core, applies one mutant, builds the named test suites against it and runs
them. At least one test must fail for every mutant ("killed"). A mutant that
survives means the tests do not guard that rule, and CI fails.

    python3 tools/mutation_check.py [--unity path/to/Unity/src]

Needs g++ and gcc. Unity's source is found as in run_native_tests.sh.
When a rule changes on purpose, update its mutant here in the same PR.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / "lib" / "rideon_core" / "src"

SAFETY = ["test_controller", "test_safety_rules"]

# (requirement, file in lib/rideon_core/src, text to replace, replacement, suites that must catch it)
MUTANTS = [
    ("SR-07", "SafetyRules.cpp", "bool cut = mode == Mode::Killed || faults.has(HARD_FAULTS);",
     "bool cut = mode == Mode::Killed;", SAFETY),
    ("SR-05", "SafetyRules.cpp", "bool cut = mode == Mode::Killed || faults.has(HARD_FAULTS);",
     "bool cut = faults.has(HARD_FAULTS);", SAFETY),
    ("SR-08", "SafetyRules.cpp", "contactorAllowed && !faults.has(CONTACTOR_FAULTS)", "contactorAllowed", SAFETY),
    ("SR-12", "SafetyRules.cpp", "contactorAllowed && !faults.has(CONTACTOR_FAULTS)",
     "!faults.has(CONTACTOR_FAULTS)", SAFETY),
    ("SR-02", "SafetyRules.cpp", "!cut && (driving || !driveStopped)", "!cut && driving", SAFETY),
    ("SR-02", "SafetyRules.cpp", "!cut && (driving || !driveStopped)", "!cut", SAFETY),
    ("SR-09", "SafetyRules.cpp", "c->amps[i] > lim.motorCurrentA", "c->amps[i] >= lim.motorCurrentA", SAFETY),
    ("SR-09", "SafetyRules.cpp", "c->driveAmps > lim.totalCurrentA", "c->driveAmps > 2 * lim.totalCurrentA", SAFETY),
    ("SR-20", "SafetyRules.cpp", "if (!c->fresh[i]) r.add", "if (false) r.add", SAFETY),
    ("SR-06", "Controller.cpp", "in.manualOk && in.estopPressed", "false", SAFETY),
    ("SR-16", "Controller.cpp", "bool parentBrake = throttle <= cfg_.kidBrakeOverride;", "bool parentBrake = false;", SAFETY),
    ("SR-15", "Controller.cpp", "kidCap(in.speedCapUs) * linkCap, cfg_.reverseCap)", "kidCap(in.speedCapUs) * linkCap, 1.0f)",
     SAFETY),
    ("SR-15", "Controller.cpp", "return pos >= 0 ? cfg_.kidCaps[pos] : cfg_.kidCaps[0];",
     "return pos >= 0 ? cfg_.kidCaps[pos] : cfg_.kidCaps[2];", SAFETY),
    ("SR-04", "Controller.cpp", "? cfg_.degradedCap : 1.0f", "? 1.0f : 1.0f", SAFETY),
    ("SR-05", "Controller.cpp", "bool killSwitch = linkUsable && killPos != 2;",
     "bool killSwitch = linkUsable && killPos == 0;", SAFETY),
    ("SR-02", "Controller.cpp", "if (out.lines.cut) {\n    ramp_.reset();", "if (false) {\n    ramp_.reset();", SAFETY),
    ("SR-02", "LinkMonitor.cpp", "if (age > p.timeoutMs) return LinkState::Lost;", "", SAFETY + ["test_link_monitor"]),
    ("SR-02", "LinkMonitor.cpp", "if (!in.linkUp || !in.anyPacket) return LinkState::Lost;",
     "if (!in.anyPacket) return LinkState::Lost;", ["test_link_monitor"]),
    ("SR-03", "Faults.h", "latched_ &= (active_ | PERMANENT_FAULTS);", "latched_ &= PERMANENT_FAULTS;",
     SAFETY + ["test_faults", "test_mode_machine"]),
    ("SR-03", "Faults.h", "    latched_ |= active;\n", "", SAFETY + ["test_faults"]),
    ("SR-09", "Faults.h", "return (uint32_t)(nowMs - startMs_) >= holdMs;", "return true;", SAFETY + ["test_faults"]),
    ("SR-14", "Faults.h", "constexpr FaultMask PERMANENT_FAULTS = FAULT_SETUP | FAULT_K1_WELDED;",
     "constexpr FaultMask PERMANENT_FAULTS = FAULT_SETUP;", SAFETY + ["test_faults"]),
    ("SR-01", "ModeMachine.h", "bool needCycle_ = true;", "bool needCycle_ = false;", SAFETY + ["test_mode_machine"]),
    ("SR-10", "ModeMachine.cpp", "if (in.request == ModeRequest::Locked && in.armingOk) {",
     "if (in.request == ModeRequest::Locked) {", SAFETY + ["test_mode_machine"]),
    ("SR-11", "ModeMachine.cpp", "mode_ = in.armingOk ? wanted : Mode::Locked;", "mode_ = wanted;",
     SAFETY + ["test_mode_machine"]),
    ("SR-05", "ModeMachine.cpp", "    mode_ = Mode::Locked;  // kill released", "    mode_ = Mode::Parent;  // kill released",
     SAFETY + ["test_mode_machine"]),
    ("SR-17", "Ramp.cpp", "float maxStep = (speedingUp ? p.accelPerS : p.brakePerS) * dtS;",
     "float maxStep = p.brakePerS * dtS;", SAFETY + ["test_ramp"]),
    ("SR-17", "Ramp.cpp", "    target = 0.0f;\n  }", "  }", SAFETY + ["test_ramp"]),
    ("SR-10", "ArmingCheck.cpp", "in.pedal < p.pedalReleased", "true", SAFETY + ["test_link_monitor"]),
    ("SR-12", "ContactorCheck.cpp", "if (busVolts < p.openMaxFraction * batteryVolts) {",
     "if (busVolts < 4 * p.openMaxFraction * batteryVolts) {", ["test_contactor"]),
    ("SR-12", "ContactorCheck.cpp", "state_ = ContactorState::Welded;", "state_ = ContactorState::Ready;",
     ["test_contactor"]),
    ("SR-19", "BatteryCheck.cpp", "volts < p.sensorMinV", "volts < 0.0f", SAFETY + ["test_faults"]),
]


def unity_src(arg):
    if arg:
        return Path(arg)
    if os.environ.get("UNITY_SRC"):
        return Path(os.environ["UNITY_SRC"])
    tag = "v2.6.1"
    dest = ROOT / ".pio" / "verify" / f"Unity-{tag}"
    if not (dest / "src" / "unity.c").exists():
        subprocess.run(["git", "clone", "--quiet", "--depth", "1", "--branch", tag,
                        "https://github.com/ThrowTheSwitch/Unity.git", str(dest)], check=True)
    return dest / "src"


def suites_fail(core: Path, suites, unity_obj: Path, unity: Path, work: Path):
    """Build and run each suite against `core`. Returns the suites that failed."""
    failed = []
    for suite in suites:
        exe = work / suite
        build = subprocess.run(
            ["g++", "-std=c++17", "-O1", "-w", f"-I{core}", f"-I{ROOT / 'include'}", f"-I{unity}",
             str(ROOT / "test" / suite / "test_main.cpp"), *map(str, sorted(core.glob("*.cpp"))), str(unity_obj),
             "-o", str(exe)], capture_output=True, text=True)
        if build.returncode != 0:
            failed.append(suite + " (build)")
            continue
        if subprocess.run([str(exe)], capture_output=True, text=True, timeout=300).returncode != 0:
            failed.append(suite)
    return failed


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--unity")
    args = ap.parse_args()
    unity = unity_src(args.unity)
    survivors = []
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        unity_obj = work / "unity.o"
        subprocess.run(["gcc", "-O1", "-c", str(unity / "unity.c"), "-o", str(unity_obj)], check=True)

        # The unmutated core must pass, or every mutant would look killed.
        clean = suites_fail(CORE, sorted({s for m in MUTANTS for s in m[4]}), unity_obj, unity, work)
        if clean:
            print("ERROR: the tests fail without any mutant:", ", ".join(clean))
            return 1

        for i, (req, name, old, new, suites) in enumerate(MUTANTS, 1):
            core = work / "core"
            if core.exists():
                shutil.rmtree(core)
            shutil.copytree(CORE, core)
            path = core / name
            text = path.read_text()
            if text.count(old) != 1:
                print(f"ERROR: mutant {i} ({req}, {name}): the text to replace is not found exactly once; update it")
                survivors.append(i)
                continue
            path.write_text(text.replace(old, new))
            caught = suites_fail(core, suites, unity_obj, unity, work)
            label = f"mutant {i:2} {req} {name}: {old.strip().splitlines()[0][:60]!r}"
            if caught:
                print(f"killed   {label}  by {', '.join(caught)}")
            else:
                print(f"SURVIVED {label}")
                survivors.append(i)
    print(f"{len(MUTANTS) - len(survivors)} of {len(MUTANTS)} mutants killed")
    return 1 if survivors else 0


if __name__ == "__main__":
    sys.exit(main())
