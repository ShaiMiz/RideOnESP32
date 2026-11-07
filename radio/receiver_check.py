#!/usr/bin/env python3
"""Read the car's ELRS receiver (Jumper AION RX Mini) over its Wi-Fi hotspot.

Works with the receiver alone on a bench supply (5.0 V, 0.2 A limit, 5V and GND only),
no radio and no ESP32 needed:

  1. Power the receiver. With no radio around, it opens the "ExpressLRS RX" Wi-Fi
     hotspot after 60 s (LED blinks fast).
  2. Join that network from this PC (password: expresslrs). The PC has no internet
     while joined.
  3. Run:  python radio/receiver_check.py   (or double-click radio/receiver_check.cmd)

It prints the receiver's product, target and firmware version, whether its binding
phrase matches the one in Documents/RideOnESP32-radio/binding-phrase.txt (creating
that file if there is none yet), and whether its automatic Wi-Fi is still on.

It also backs the receiver up to Documents/RideOnESP32-radio/receiver-backup-<date>/:
the running firmware as the receiver hands it out (/firmware.bin: the image with its
options and hardware layout appended, so it can be flashed back as is), plus
config.json, options.json and hardware.json. To restore, flash that .bin from the
receiver's web page (Update) or the Configurator's local-file option.
It only reads; it changes nothing on the receiver.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import install_radio  # noqa: E402

DEFAULT_HOST = "10.0.0.1"


def phrase_uid(phrase: str) -> list[int]:
    """The 6-byte UID ExpressLRS derives from a binding phrase (as its build tools do)."""
    return list(hashlib.md5(('-DMY_BINDING_PHRASE="' + phrase + '"').encode()).digest()[:6])


def fetch(host: str, path: str, timeout: float) -> dict | None:
    try:
        with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
            return json.loads(r.read().decode("utf-8", errors="replace"))
    except (urllib.error.URLError, OSError, ValueError):
        return None


def download(host: str, path: str, timeout: float) -> tuple[bytes, str] | None:
    """Raw bytes and the file name the receiver suggests, or None."""
    try:
        with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
            data = r.read()
            name = ""
            disp = r.headers.get("Content-Disposition") or ""
            if "filename=" in disp:
                name = disp.split("filename=", 1)[1].strip().strip('"')
            return data, name
    except (urllib.error.URLError, OSError):
        return None


def backup(host: str, folder: Path, info: dict, firmware: bool = True) -> list[str]:
    """Save the receiver's firmware and settings; return what was saved or why not."""
    folder.mkdir(parents=True, exist_ok=True)
    lines = []
    (folder / "config.json").write_text(json.dumps(info, indent=2), encoding="utf-8")
    lines.append("config.json (settings, options, binding UID)")
    for name in ("options.json", "hardware.json"):
        got = download(host, "/" + name, 10)
        if got and got[0].strip():
            (folder / name).write_bytes(got[0])
            lines.append(name)
    if firmware:
        got = download(host, "/firmware.bin", 120)
        if got is None:
            lines.append("firmware: this receiver's firmware does not hand out its image")
        else:
            data, name = got
            s = info["settings"]
            name = Path(name).name or f"{s.get('target', 'receiver')}_{s.get('version', 'unknown')}.bin"
            if len(data) < 64 * 1024 or data[0] != 0xE9:   # 0xE9 = ESP firmware image header
                lines.append(f"firmware: download did not look like a firmware image ({len(data)} bytes), not saved")
            else:
                (folder / name).write_bytes(data)
                lines.append(f"{name} ({len(data) // 1024} KB firmware)")
    return lines


def read_receiver(host: str, timeout: float = 5.0) -> dict | None:
    cfg = fetch(host, "/config", timeout)
    if cfg is None:
        return None
    target = fetch(host, "/target", timeout) or {}   # ExpressLRS 3.x keeps the version here
    settings = dict(target)
    settings.update(cfg.get("settings") or {})
    return {"settings": settings, "options": cfg.get("options") or {}, "config": cfg.get("config") or {}}


def report(info: dict, phrase: str) -> tuple[list[str], list[str]]:
    """Lines to print, and the problems among them."""
    s, o, c = info["settings"], info["options"], info["config"]
    lines, problems = [], []
    name = s.get("product_name") or s.get("lua_name") or "?"
    version = s.get("version") or "?"
    lines.append(f"Receiver: {name}")
    lines.append(f"Target: {s.get('target', '?')}   firmware: {version}"
                 + (f" ({s['git-commit']})" if s.get("git-commit") else ""))
    want = install_radio.parse_version(install_radio.RECOMMENDED_ELRS)
    have = install_radio.parse_version(str(version))
    if have is None:
        problems.append("could not read the firmware version")
    elif have != want:
        problems.append(f"firmware {version}: flash {install_radio.RECOMMENDED_ELRS} (radio/README.md step 5)")

    uid = list(c.get("uid") or o.get("uid") or [])
    if not uid:
        problems.append("no binding UID reported")
    elif uid == phrase_uid(phrase):
        lines.append("Binding phrase: matches binding-phrase.txt")
    else:
        problems.append("binding phrase is not ours: flash with the phrase from binding-phrase.txt")

    interval = o.get("wifi-on-interval")
    if interval in (None, -1):
        lines.append("Automatic Wi-Fi: off")
    else:
        problems.append(f"automatic Wi-Fi is on (after {interval} s): untick AUTO_WIFI_ON_INTERVAL when flashing")

    for p in problems:
        lines.append(f"TO DO: {p}")
    if not problems:
        lines.append("All good: version, binding phrase and Wi-Fi setting are as planned.")
    return lines, problems


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Read the ELRS receiver over its Wi-Fi hotspot.")
    ap.add_argument("--host", default=DEFAULT_HOST, help="receiver address (default 10.0.0.1)")
    ap.add_argument("--home", type=Path, default=None,
                    help="where the binding phrase and reports live (default: Documents/RideOnESP32-radio)")
    ap.add_argument("--no-firmware", action="store_true", help="back up the settings only")
    args = ap.parse_args(argv)
    home = args.home or (install_radio.documents_dir() / "RideOnESP32-radio")

    phrase, new = install_radio.binding_phrase(home / "binding-phrase.txt")
    print(f"Binding phrase ({'new, ' if new else ''}from {home / 'binding-phrase.txt'}):\n  {phrase}\n")

    info = read_receiver(args.host)
    if info is None:
        print(f"No receiver at {args.host}. Power it (5.0 V, 5V and GND only), wait 60 s for the "
              "\"ExpressLRS RX\" network, join it (password expresslrs), then run this again.")
        return 2

    lines, problems = report(info, phrase)
    print("\n".join(lines))
    home.mkdir(parents=True, exist_ok=True)
    folder = home / f"receiver-backup-{_dt.datetime.now():%Y%m%d-%H%M%S}"
    print(f"\nBacking up to {folder} (the firmware takes up to a minute):")
    for line in backup(args.host, folder, info, firmware=not args.no_firmware):
        print("  " + line)
    print("Copy the firmware version into the version log in docs/hardware/DEVICE_FIRMWARE.md.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
