#!/usr/bin/env python3
"""Summarise the T-Lite's telemetry logs (range and reliability tests).

The RideOn model logs telemetry to LOGS/RideOn-<date>.csv twice a second whenever the
model is loaded. install_radio.py copies new logs to the PC and prints this summary;
you can also run it on a folder or file:

    python radio/radio_logs.py <SD card or LOGS folder or .csv>

Per log: length, link quality (min / 5th percentile / median), the time spent below
70 % (where the car halves its speed cap) and below 40 % (counted as lost), telemetry
dropouts (link lost and back), the weakest signal and its SNR, transmit power, the
lowest battery voltage, and every mode or fault text the car sent.
Pass/fail lines follow the range gates in radio/README.md.
"""
from __future__ import annotations

import csv
import statistics
import sys
from dataclasses import dataclass, field
from pathlib import Path

LQ_DEGRADED = 70   # config.h LQ_DEGRADED_BELOW
LQ_LOST = 40       # config.h LQ_LOST_BELOW


def _col(header: list[str], name: str) -> int | None:
    for i, h in enumerate(header):
        if h.split("(")[0].strip() == name:
            return i
    return None


def _num(row: list[str], i: int | None) -> float | None:
    if i is None or i >= len(row) or row[i].strip() == "":
        return None
    try:
        return float(row[i])
    except ValueError:
        return None


def _seconds(t: str) -> float | None:
    try:
        h, m, s = t.split(":")
        return int(h) * 3600 + int(m) * 60 + float(s)
    except ValueError:
        return None


@dataclass
class Summary:
    path: Path
    seconds: float = 0.0
    samples: int = 0
    lq: list[float] = field(default_factory=list)
    rssi_min: float | None = None
    snr_at_rssi_min: float | None = None
    tpwr_max: float | None = None
    batt_min: float | None = None
    dropouts: int = 0
    longest_dropout_s: float = 0.0
    fm_seen: list[str] = field(default_factory=list)
    below_degraded_s: float = 0.0
    below_lost_s: float = 0.0

    def lines(self) -> list[str]:
        out = [f"{self.path.name}: {self.seconds / 60:.1f} min, {self.samples} samples"]
        if not self.lq:
            out.append("  no link data (the car was off the whole time)")
            return out
        q = sorted(self.lq)
        p5 = q[max(0, int(len(q) * 0.05) - 1)]
        out.append(f"  link quality: min {q[0]:.0f} %, 5th percentile {p5:.0f} %, median {statistics.median(q):.0f} %")
        out.append(f"  below {LQ_DEGRADED} %: {self.below_degraded_s:.1f} s, below {LQ_LOST} %: {self.below_lost_s:.1f} s")
        out.append(f"  link drops: {self.dropouts} (longest {self.longest_dropout_s:.1f} s)")
        if self.rssi_min is not None:
            snr = f", SNR {self.snr_at_rssi_min:.0f} dB" if self.snr_at_rssi_min is not None else ""
            out.append(f"  weakest signal: {self.rssi_min:.0f} dBm{snr}")
        if self.tpwr_max is not None:
            out.append(f"  highest transmit power: {self.tpwr_max:.0f} mW")
        if self.batt_min is not None:
            out.append(f"  lowest car battery: {self.batt_min:.1f} V")
        if self.fm_seen:
            out.append("  car status seen: " + ", ".join(self.fm_seen))
        faults = [f for f in self.fm_seen if " " in f]
        verdict = "PASS" if self.dropouts == 0 and q[0] >= LQ_DEGRADED and not faults else "CHECK"
        out.append(f"  {verdict}: no drops, link quality never under {LQ_DEGRADED} %, no faults")
        return out


def summarise(path: Path) -> Summary:
    s = Summary(path)
    with path.open(newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.reader(f))
    if not rows:
        return s
    h = rows[0]
    c_time, c_lq, c_rssi, c_snr = _col(h, "Time"), _col(h, "RQly"), _col(h, "1RSS"), _col(h, "RSNR")
    c_tpwr, c_bat, c_fm = _col(h, "TPWR"), _col(h, "RxBt"), _col(h, "FM")
    first = last = prev_t = None
    link_was_up = None
    drop_start = None
    for row in rows[1:]:
        t = _seconds(row[c_time]) if c_time is not None and c_time < len(row) else None
        if t is None:
            continue
        if first is None:
            first = t
        dt = (t - prev_t) if prev_t is not None and t >= prev_t else 0.0
        prev_t = last = t
        s.samples += 1
        lq = _num(row, c_lq)
        up = lq is not None and lq > 0
        if up:
            s.lq.append(lq)
            if lq < LQ_DEGRADED:
                s.below_degraded_s += dt
            if lq < LQ_LOST:
                s.below_lost_s += dt
            rssi = _num(row, c_rssi)
            if rssi is not None and (s.rssi_min is None or rssi < s.rssi_min):
                s.rssi_min, s.snr_at_rssi_min = rssi, _num(row, c_snr)
            p = _num(row, c_tpwr)
            if p is not None:
                s.tpwr_max = p if s.tpwr_max is None else max(s.tpwr_max, p)
            b = _num(row, c_bat)
            if b is not None and b > 1 and (s.batt_min is None or b < s.batt_min):
                s.batt_min = b
            if c_fm is not None and c_fm < len(row):
                fm = row[c_fm].strip()
                if fm and fm not in s.fm_seen:
                    s.fm_seen.append(fm)
        if link_was_up and not up:
            s.dropouts += 1
            drop_start = t
        if up and drop_start is not None:
            s.longest_dropout_s = max(s.longest_dropout_s, t - drop_start)
            drop_start = None
        link_was_up = up
    if first is not None and last is not None:
        s.seconds = max(0.0, last - first)
    # A drop that never came back is the car being switched off, not a dropout.
    if drop_start is not None and s.dropouts:
        s.dropouts -= 1
    return s


def find_logs(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    logs = target / "LOGS" if (target / "LOGS").is_dir() else target
    return sorted(logs.glob("RideOn-*.csv"))


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print(__doc__)
        return 2
    logs = find_logs(Path(argv[0]))
    if not logs:
        print("No RideOn-*.csv logs found.")
        return 1
    for p in logs:
        print("\n".join(summarise(p).lines()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
