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
that file if there is none yet), and whether its automatic Wi-Fi is still on. It saves
everything to Documents/RideOnESP32-radio/receiver-<date>.json. It only reads; it
changes nothing on the receiver.
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
    out = home / f"receiver-{_dt.datetime.now():%Y%m%d-%H%M%S}.json"
    out.write_text(json.dumps(info, indent=2), encoding="utf-8")
    print(f"\nSaved {out}")
    print("Copy the firmware version into the version log in docs/hardware/DEVICE_FIRMWARE.md.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
