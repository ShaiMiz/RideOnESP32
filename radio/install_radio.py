#!/usr/bin/env python3
"""Install the RideOn model and scripts on the Jumper T-Lite V2's SD card.

Plug the radio into the PC with USB, choose "USB Storage (SD)" on the radio, then run:

    python radio/install_radio.py          (or double-click radio/install_radio.cmd)

It:
  1. finds the radio's SD card (a drive with RADIO/radio.yml),
  2. backs up the whole card to Documents/RideOnESP32-radio/backups/,
  3. reads /RideOn-radio.txt (written by the radio's "RideOn ELRS setup" tool) and
     shows the exact radio, ELRS module and receiver versions,
  4. copies new telemetry logs (LOGS/RideOn-*.csv) to the PC and summarises the link
     quality, signal, drops and faults in each (radio_logs.py),
  5. checks the EdgeTX version and stick mode,
  6. copies the RideOn model and the two Lua scripts, and makes RideOn the current model,
  7. creates the ELRS binding phrase once (kept on the PC, never in git) and prints it.

Nothing on the card is deleted. Run it again after any change in radio/edgetx/sdcard/.
Uses only the Python standard library (PlatformIO's Python works).
"""
from __future__ import annotations

import argparse
import datetime as _dt
import filecmp
import os
import re
import secrets
import shutil
import string
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import radio_logs  # noqa: E402

HERE = Path(__file__).resolve().parent
SDCARD = HERE / "edgetx" / "sdcard"
MODEL_SRC = SDCARD / "MODELS" / "model42.yml"
SCRIPTS = [
    Path("SCRIPTS/TELEMETRY/rideon.lua"),
    Path("SCRIPTS/TOOLS/RideOnRF.lua"),
]
REPORT = "RideOn-radio.txt"

# Versions this repo was checked against (radio/README.md). Older works may still work,
# but update first. EdgeTX 2.12 dropped the T-Lite, so 2.11.8 is the last release for it;
# the model file uses only keys that 2.11.8 and 2.12.5 both read.
MIN_EDGETX = (2, 10, 0)
RECOMMENDED_EDGETX = "2.11.8"
RECOMMENDED_ELRS = "4.1.0"  # radio and receiver must run the same major version (3.x does not link with 4.x)

WORDS = (
    "amber anchor apple arrow aspen badger bamboo banjo basil beacon birch bison bramble "
    "cactus canyon cedar cherry cobalt comet copper coral cricket dahlia delta dune ember "
    "falcon fern fjord forest fox garnet gecko ginger glacier granite harbor hazel heron "
    "indigo iris jasper juniper kayak kiwi lagoon lantern lemon lilac lynx maple marble "
    "meadow mango nectar nimbus nutmeg oasis olive onyx orchid otter pebble pepper pine "
    "plum quartz quill raven reef river saffron sage salmon sequoia sparrow spruce summit "
    "tango thistle tiger topaz tulip tundra umber velvet violet walnut willow yarrow zephyr"
).split()


def say(msg: str = "") -> None:
    print(msg, flush=True)


def documents_dir() -> Path:
    home = Path.home()
    docs = home / "Documents"
    return docs if docs.is_dir() else home


def find_sd_cards() -> list[Path]:
    """Every mounted drive that looks like an EdgeTX SD card."""
    roots: list[Path] = []
    if os.name == "nt":
        roots = [Path(f"{letter}:/") for letter in string.ascii_uppercase]
    else:
        for base in ("/media", "/run/media", "/Volumes", "/mnt"):
            b = Path(base)
            if b.is_dir():
                roots += [p for p in b.glob("*")] + [p for p in b.glob("*/*")]
    found = []
    for r in roots:
        try:
            if (r / "RADIO" / "radio.yml").is_file():
                found.append(r)
        except OSError:
            continue
    return found


def parse_version(text: str) -> tuple[int, int, int] | None:
    m = re.search(r"(\d+)\.(\d+)\.(\d+)", text or "")
    return tuple(int(x) for x in m.groups()) if m else None


def read_radio_yml(sd: Path) -> str:
    return (sd / "RADIO" / "radio.yml").read_text(encoding="utf-8", errors="replace")


def yml_value(text: str, key: str) -> str | None:
    m = re.search(rf"^{re.escape(key)}:\s*\"?([^\"\r\n]*)\"?\s*$", text, re.MULTILINE)
    return m.group(1).strip() if m else None


def model_name(path: Path) -> str | None:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    in_header = False
    for line in text.splitlines():
        if not line.strip():
            continue
        if not line[0].isspace():
            in_header = line.rstrip() == "header:"
            continue
        if in_header and line.strip().startswith("name:"):
            return line.split(":", 1)[1].strip().strip('"').strip()
    return None


def choose_model_file(sd: Path) -> Path:
    """The existing RideOn model if there is one, else model42.yml, else the first free name."""
    models = sd / "MODELS"
    for p in sorted(models.glob("model*.yml")):
        if model_name(p) == "RideOn":
            return p
    preferred = models / MODEL_SRC.name
    if not preferred.exists():
        return preferred
    for n in range(1, 100):
        p = models / f"model{n:02d}.yml"
        if not p.exists():
            return p
    raise SystemExit("MODELS/ has no free model file name; delete an unused model on the radio first.")


def backup(sd: Path, dest_dir: Path) -> Path:
    dest_dir.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = dest_dir / f"tlite-sd-{stamp}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sd.rglob("*"):
            if p.is_file() and "System Volume Information" not in p.parts:
                try:
                    z.write(p, p.relative_to(sd).as_posix())
                except OSError as e:
                    say(f"  (skipped {p}: {e})")
    return out


def binding_phrase(path: Path) -> tuple[str, bool]:
    if path.is_file():
        phrase = path.read_text(encoding="utf-8").strip()
        if phrase:
            return phrase, False
    phrase = "rideon-" + "-".join(secrets.choice(WORDS) for _ in range(5))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(phrase + "\n", encoding="utf-8")
    return phrase, True


def read_report(sd: Path) -> dict[str, str]:
    p = sd / REPORT
    if not p.is_file():
        return {}
    out = {}
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            out[k.strip()] = v.strip()
    return out


def check_versions(radio_yml: str, report: dict[str, str]) -> list[str]:
    """Warnings about versions, or [] when everything is as recommended."""
    warn = []
    etx = parse_version(report.get("edgetx", "")) or parse_version(yml_value(radio_yml, "semver") or "")
    if etx is None:
        warn.append("Could not read the EdgeTX version.")
    elif etx < MIN_EDGETX:
        warn.append(f"EdgeTX {'.'.join(map(str, etx))} is too old: update to {RECOMMENDED_EDGETX} first "
                    "(EdgeTX Buddy, radio/README.md step 2).")
    elif etx < parse_version(RECOMMENDED_EDGETX):
        warn.append(f"EdgeTX {'.'.join(map(str, etx))}: {RECOMMENDED_EDGETX} is recommended.")
    vers = {}
    for key in ("elrs_tx", "elrs_rx"):
        v = parse_version(report.get(key, ""))
        if v:
            vers[key] = v
            if v < parse_version(RECOMMENDED_ELRS):
                warn.append(f"{key} runs ELRS {'.'.join(map(str, v))}: flash {RECOMMENDED_ELRS} (radio/README.md step 4).")
    if len(vers) == 2 and vers["elrs_tx"] != vers["elrs_rx"]:
        warn.append("The radio's ELRS module and the receiver run different versions: flash both to the same one.")
    mode = yml_value(radio_yml, "stickMode")
    if mode is not None and mode != "1":
        warn.append(f"The radio is in stick mode {int(mode) + 1}, not mode 2. In mode 2 the right stick "
                    "steers and drives; change it in SYS > Radio setup > Mode before driving.")
    return warn


def collect_logs(sd: Path, dest: Path, dry: bool) -> list[Path]:
    """Copy RideOn logs that are new or grew since the last run; return them."""
    out = []
    for p in radio_logs.find_logs(sd):
        d = dest / p.name
        if d.exists() and d.stat().st_size == p.stat().st_size:
            continue
        if dry:
            out.append(p)
            continue
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, d)
        out.append(d)
    return out


def set_current_model(sd: Path, filename: str, dry: bool) -> bool:
    p = sd / "RADIO" / "radio.yml"
    text = p.read_text(encoding="utf-8", errors="replace")
    new, n = re.subn(r"^currModelFilename:.*$", f'currModelFilename: "{filename}"', text, flags=re.MULTILINE)
    if n != 1:
        return False
    if new != text and not dry:
        p.write_text(new, encoding="utf-8", newline="")
    return True


def copy(src: Path, dst: Path, dry: bool) -> str:
    existed = dst.exists()
    if existed and filecmp.cmp(src, dst, shallow=False):
        return "unchanged"
    if not dry:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    return "updated" if existed else "added"


def install(sd: Path, *, backup_dir: Path | None, phrase_file: Path, dry: bool) -> int:
    say(f"Radio SD card: {sd}")
    radio_yml = read_radio_yml(sd)

    report = read_report(sd)
    if report:
        say("\nLast report from the radio's \"RideOn ELRS setup\" tool:")
        for k, v in report.items():
            say(f"  {k}: {v}")
        if backup_dir and not dry:
            backup_dir.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(sd / REPORT, backup_dir.parent / f"radio-report-{_dt.date.today()}.txt")
    else:
        say("\nNo report from the radio yet (run SYS > Tools > RideOn ELRS setup after this).")

    if backup_dir:
        new_logs = collect_logs(sd, backup_dir.parent / "logs", dry)
        if new_logs:
            say("\nNew telemetry logs:")
            for p in new_logs:
                for line in radio_logs.summarise(p).lines():
                    say("  " + line)

    warnings = check_versions(radio_yml, report)
    for w in warnings:
        say(f"WARNING: {w}")

    if backup_dir and not dry:
        say("\nBacking up the SD card...")
        say(f"  {backup(sd, backup_dir)}")

    say("\nInstalling:")
    target = choose_model_file(sd)
    say(f"  MODELS/{target.name}: {copy(MODEL_SRC, target, dry)}")
    for rel in SCRIPTS:
        say(f"  {rel.as_posix()}: {copy(SDCARD / rel, sd / rel, dry)}")
    if set_current_model(sd, target.name, dry):
        say(f"  RideOn is now the current model ({target.name})")
    else:
        say("  Could not set the current model: select RideOn in the model list.")

    phrase, new = binding_phrase(phrase_file)
    say(f"\nELRS binding phrase ({'new, ' if new else ''}saved in {phrase_file}):")
    say(f"  {phrase}")
    say("  Paste it into the ExpressLRS Configurator for BOTH the radio module and the receiver.")

    say("\nNext: eject the drive, unplug USB, and follow radio/README.md from the step after this one.")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--sd", type=Path, help="SD card root (default: search the drives)")
    ap.add_argument("--no-backup", action="store_true", help="skip the SD card backup")
    ap.add_argument("--dry-run", action="store_true", help="show what would change, change nothing")
    ap.add_argument("--home", type=Path, default=None,
                    help="where backups and the binding phrase live (default: Documents/RideOnESP32-radio)")
    args = ap.parse_args(argv)

    home = args.home or (documents_dir() / "RideOnESP32-radio")
    if args.sd:
        sd = args.sd
        if not (sd / "RADIO" / "radio.yml").is_file():
            say(f"{sd} has no RADIO/radio.yml: is it the radio's SD card?")
            return 2
    else:
        cards = find_sd_cards()
        if not cards:
            say("No radio SD card found. Plug the T-Lite into USB, choose \"USB Storage (SD)\" on its "
                "screen, wait for the drive to appear, then run this again.")
            return 2
        if len(cards) > 1:
            say("More than one radio SD card found; pick one with --sd: " + ", ".join(map(str, cards)))
            return 2
        sd = cards[0]

    return install(sd, backup_dir=None if args.no_backup else home / "backups",
                   phrase_file=home / "binding-phrase.txt", dry=args.dry_run)


if __name__ == "__main__":
    sys.exit(main())
