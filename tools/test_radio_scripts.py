#!/usr/bin/env python3
"""Run the radio's Lua scripts and the PC installer against simulations.

- rideon.lua (telemetry screen + alarms) runs in Lua 5.2 (as in EdgeTX) with a mock
  EdgeTX API: screen without link, mode and fault alerts, low battery, weak link.
- RideOnRF.lua (ELRS auto-setup) talks to a simulated ELRS 3.x TX module that answers
  the CRSF parameter protocol like the real one, including refusing a packet-rate or
  switch-mode change while connected and chunked parameter replies.
- install_radio.py runs against a fake SD card.

Needs: pip install lupa pyyaml (versions in .github/workflows/build.yml). Run from the repo root: python3 tools/test_radio_scripts.py
"""
from __future__ import annotations

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from lupa import lua52

ROOT = Path(__file__).resolve().parent.parent
SD = ROOT / "radio/edgetx/sdcard"
sys.path.insert(0, str(ROOT / "radio"))
import install_radio  # noqa: E402
import radio_logs  # noqa: E402

CONSTS = dict(INVERS=1, BLINK=2, SMLSIZE=4, MIDSIZE=8, DBLSIZE=16, PLAY_NOW=1,
              PLAY_BACKGROUND=2, UNIT_VOLTS=1, PREC1=32, EVT_VIRTUAL_EXIT=99)


class Radio:
    """Mock EdgeTX: clock, sources, screen, sounds, CRSF queue, SD files."""

    def __init__(self):
        self.lua = lua52.LuaRuntime(unpack_returned_tuples=True)
        self.t = 0
        self.values = {"sa": -1024, "sb": -1024, "sc": -1024}
        self.screen: list[str] = []
        self.sounds: list[tuple] = []
        self.pushed: list[tuple] = []
        self.rx_queue: list[tuple] = []
        self.files: dict[str, str] = {}
        self.device = None
        g = self.lua.globals()
        for k, v in CONSTS.items():
            g[k] = v
        g.getTime = lambda: self.t
        g.getValue = lambda name: self.values.get(name, 0)
        g.playTone = lambda *a: self.sounds.append(("tone",) + tuple(a))
        g.playHaptic = lambda *a: self.sounds.append(("haptic",) + tuple(a))
        g.playNumber = lambda *a: self.sounds.append(("number",) + tuple(a))
        g.getVersion = lambda: ("2.12.5", "tlitef4", 2, 12, 5)
        g.getDateTime = lambda: self.lua.table_from(dict(year=2026, mon=11, day=7, hour=12, min=0))
        lcd = self.lua.table_from({})
        lcd.clear = lambda: self.screen.clear()
        lcd.drawText = lambda x, y, text, flags=0: self.screen.append(str(text))
        g.lcd = lcd
        g.crossfireTelemetryPush = self._push
        g.crossfireTelemetryPop = self._pop
        fs = self.lua.table_from({})
        fs.open = self._open
        fs.write = lambda f, *parts: f.__setitem__("buf", f["buf"] + "".join(str(p) for p in parts))
        fs.close = lambda f: self.files.__setitem__(f["path"], f["buf"])
        g.io = fs

    def _open(self, path, mode):
        return self.lua.table_from({"path": path, "buf": ""})

    def _push(self, cmd, data):
        frame = [data[i] for i in range(1, len(data) + 1)]
        self.pushed.append((cmd, frame))
        if self.device:
            self.rx_queue += self.device.handle(cmd, frame)
        return True

    def _pop(self):
        if not self.rx_queue:
            return None
        cmd, frame = self.rx_queue.pop(0)
        return cmd, self.lua.table_from(frame)

    def load(self, rel):
        return self.lua.execute((SD / rel).read_text())

    def text(self):
        return " | ".join(self.screen)


def cstr(s):
    return list(s.encode()) + [0]


class ElrsTx:
    """A simulated ELRS 4.1.0 TX module (0xEE) and, when the car is on, its receiver."""

    RATES = ("50Hz(-115dBm);100Hz Full(-112dBm);150Hz(-112dBm);250Hz(-108dBm);333Hz Full(-105dBm);"
             "500Hz(-105dBm);D250(-104dBm);D500(-104dBm);F500(-104dBm);F1000(-104dBm)")

    def __init__(self, connected=False):
        self.connected = connected
        self.rx_model_id = 0xFF
        self.model_id = 42
        self.error_connected = False
        # id: [parent, type, name, options, value, unit]
        self.fields = {
            1: [0, 9, "Packet Rate", self.RATES, 5, ""],
            2: [0, 9, "Telem Ratio", "Std;Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2;Race", 0, ""],
            3: [0, 11, "TX Power", None, 0, ""],
            4: [3, 9, "Max Power", "10;25;50;100;250", 1, "mW"],
            5: [3, 9, "Dynamic", "Off;Dyn;AUX9;AUX10;AUX11;AUX12", 0, ""],
            6: [0, 9, "Switch Mode", "Wide;Hybrid", 1, ""],
            7: [0, 9, "Model Match", "Off;On", 0, ""],
            8: [0, 13, "Bind", None, 0, ""],
        }
        self.writes: list[tuple[int, int]] = []

    def value(self, name):
        f = next(f for f in self.fields.values() if f[2] == name)
        return f[3].split(";")[f[4]]

    def payload(self, fid):
        parent, typ, name, opts, value, unit = self.fields[fid]
        p = [parent, typ] + cstr(name)
        if typ == 9:
            p += cstr(opts) + [value, 0, len(opts.split(";")) - 1, 0] + cstr(unit)
        elif typ == 13:
            p += [0, 50] + cstr("")
        return p

    def handle(self, cmd, f):
        out = []
        if cmd == 0x28:
            out.append((0x29, [0xEA, 0xEE] + cstr("Jumper T-Lite 2.4G TX") + [0x45, 0x4C, 0x52, 0x53]
                        + [0] * 4 + [0, 4, 1, 0] + [len(self.fields), 0]))
            if self.connected:
                out.append((0x29, [0xEA, 0xEC] + cstr("Jumper AION RX Mini") + [0x45, 0x4C, 0x52, 0x53]
                            + [0] * 4 + [0, 4, 1, 0] + [12, 0]))
        elif cmd == 0x2C and f[0] == 0xEE:
            fid, chunk = f[2], f[3]
            data = self.payload(fid)
            size = 20  # small chunks, to test reassembly
            parts = [data[i:i + size] for i in range(0, len(data), size)] or [[]]
            remaining = len(parts) - 1 - chunk
            out.append((0x2B, [0xEF, 0xEE, fid, remaining] + parts[chunk]))
        elif cmd == 0x2D and f[0] == 0xEE:
            fid, val = f[2], f[3]
            if fid == 0:
                flags = (1 if self.connected else 0)
                if self.connected and self.fields[7][4] == 1 and self.rx_model_id not in (0xFF, self.model_id):
                    flags |= 4
                if self.error_connected:
                    flags |= 32
                out.append((0x2E, [0xEF, 0xEE, 0, 0, 100, flags] + cstr("")))
                return out
            self.writes.append((fid, val))
            name = self.fields[fid][2]
            if name in ("Packet Rate", "Switch Mode") and self.connected:
                self.error_connected = True   # ELRS refuses while connected
                return out
            self.fields[fid][4] = val
            if name == "Model Match" and self.connected:
                self.rx_model_id = self.model_id if val == 1 else 0xFF
        return out


def run_tool(radio, frames=600, step=5):
    tool = radio.load("SCRIPTS/TOOLS/RideOnRF.lua")
    tool.init()
    for _ in range(frames):
        radio.t += step
        tool.run(0)
    return tool


class ToolScript(unittest.TestCase):
    def test_refuses_unless_kill_and_locked(self):
        r = Radio()
        r.device = ElrsTx()
        r.values.update(sc=1024)  # RUN
        run_tool(r, frames=200)
        self.assertEqual(r.pushed, [])
        self.assertIn("KILL", r.text())

    def test_car_off_sets_link_settings(self):
        r = Radio()
        r.device = dev = ElrsTx(connected=False)
        run_tool(r)
        self.assertEqual(dev.value("Packet Rate"), "150Hz(-112dBm)")
        self.assertEqual(dev.value("Telem Ratio"), "1:8")
        self.assertEqual(dev.value("Max Power"), "100")
        self.assertEqual(dev.value("Dynamic"), "Dyn")
        self.assertEqual(dev.value("Switch Mode"), "Hybrid")
        self.assertEqual(dev.value("Model Match"), "Off")  # needs the car on
        rep = r.files["/RideOn-radio.txt"]
        self.assertIn("elrs_tx: Jumper T-Lite 2.4G TX 4.1.0", rep)
        self.assertIn("elrs_rx: not found", rep)
        self.assertIn("Packet Rate: 150Hz ok", rep)
        self.assertIn("Model Match: Off, run again car ON", rep)
        self.assertIn("edgetx: 2.12.5", rep)

    def test_car_on_teaches_model_match_and_reads_receiver(self):
        r = Radio()
        r.device = dev = ElrsTx(connected=True)
        dev.fields[1][4] = 2  # already 150Hz from the car-off run
        dev.fields[2][4] = 6
        dev.fields[4][4] = 3
        dev.fields[5][4] = 1
        run_tool(r)
        self.assertEqual(dev.value("Model Match"), "On")
        self.assertEqual(dev.rx_model_id, 42)
        rep = r.files["/RideOn-radio.txt"]
        self.assertIn("elrs_rx: Jumper AION RX Mini 4.1.0", rep)
        self.assertIn("receiver_connected: yes", rep)
        self.assertIn("model_match_ok: yes", rep)
        self.assertIn("Model Match: On ok", rep)
        self.assertNotIn(1, [fid for fid, _ in dev.writes])  # no rate write needed

    def test_car_on_does_not_try_rate_change(self):
        r = Radio()
        r.device = dev = ElrsTx(connected=True)
        run_tool(r)
        self.assertNotIn(1, [fid for fid, _ in dev.writes])
        self.assertFalse(dev.error_connected)
        self.assertIn("Packet Rate: 500Hz, run again car OFF", r.files["/RideOn-radio.txt"])

    def test_switch_moved_mid_run_stops_writes(self):
        r = Radio()
        r.device = dev = ElrsTx()
        tool = r.load("SCRIPTS/TOOLS/RideOnRF.lua")
        tool.init()
        for _ in range(35):  # past the ping, into reading settings
            r.t += 5
            tool.run(0)
        r.values["sa"] = 0   # Kid
        for _ in range(500):
            r.t += 5
            tool.run(0)
        self.assertEqual(dev.writes, [])
        self.assertIn("Locked", r.text())

    def test_no_module(self):
        r = Radio()
        run_tool(r, frames=400)
        self.assertIn("No ELRS module answers", r.text())

    def test_exit(self):
        r = Radio()
        tool = r.load("SCRIPTS/TOOLS/RideOnRF.lua")
        self.assertEqual(tool.run(CONSTS["EVT_VIRTUAL_EXIT"]), 1)


class TelemetryScript(unittest.TestCase):
    def setUp(self):
        self.r = Radio()
        self.s = self.r.load("SCRIPTS/TELEMETRY/rideon.lua")

    def tick(self, n=1, dt=10):
        for _ in range(n):
            self.r.t += dt
            self.s.background()

    def link(self, **v):
        self.r.values.update({"RQly": 100, "RxBt": 24.5, "Curr": 3.2, "Capa": 120, "1RSS": -40,
                              "TPWR": 25, "FM": "LOCKED"})
        self.r.values.update(v)

    def test_no_link_screen(self):
        self.s.run(0)
        self.assertIn("NO LINK", self.r.text())

    def test_screen_with_link(self):
        self.link(FM="PARENT")
        self.s.run(0)
        t = self.r.text()
        for part in ("PARENT", "24.5V", "3.2A", "LQ 100%", "KILL Lock 30%"):
            self.assertIn(part, t)

    def test_fault_alert(self):
        self.link(FM="PARENT")
        self.tick()
        self.r.sounds.clear()
        self.r.values["FM"] = "LOCKED ESTOP"
        self.tick()
        self.assertTrue(any(s[0] == "haptic" for s in self.r.sounds))
        self.s.run(0)
        self.assertIn("ESTOP", self.r.text())

    def test_mode_beeps(self):
        self.link(FM="LOCKED")
        self.tick()
        self.r.sounds.clear()
        self.r.values["FM"] = "PARENT"
        self.tick()
        self.assertEqual(sum(1 for s in self.r.sounds if s[0] == "tone"), 3)

    def test_low_battery_repeats_every_10s(self):
        self.link(RxBt=21.5)
        self.tick()
        self.assertIn(("number", 215, 1, 32), self.r.sounds)
        n = sum(1 for s in self.r.sounds if s[0] == "number")
        self.tick(50)       # 5 s: no repeat yet
        self.assertEqual(sum(1 for s in self.r.sounds if s[0] == "number"), n)
        self.tick(60)       # past 10 s
        self.assertEqual(sum(1 for s in self.r.sounds if s[0] == "number"), n + 1)

    def test_weak_link(self):
        self.link(RQly=60)
        self.tick()
        self.assertTrue(any(s[0] == "tone" and s[1] == 2200 for s in self.r.sounds))

    def test_link_lost_buzz(self):
        self.link()
        self.tick()
        self.r.sounds.clear()
        self.r.values["RQly"] = 0
        self.tick()
        self.assertTrue(any(s[0] == "haptic" for s in self.r.sounds))


def fake_sd(root: Path, stick_mode=1, semver="2.12.5", extra_models=()):
    (root / "RADIO").mkdir(parents=True)
    (root / "MODELS").mkdir()
    (root / "RADIO/radio.yml").write_text(
        f"semver: {semver}\nboard: tlitef4\nstickMode: {stick_mode}\ncurrModelFilename: \"model01.yml\"\n")
    (root / "MODELS/model01.yml").write_text('semver: 2.12.5\nheader:\n  name: "Model01"\n')
    for name, model in extra_models:
        (root / "MODELS" / name).write_text(f'semver: 2.12.5\nheader:\n  name: "{model}"\n')


class Installer(unittest.TestCase):
    def run_install(self, sd, home, *extra):
        out = io.StringIO()
        with redirect_stdout(out):
            rc = install_radio.main(["--sd", str(sd), "--home", str(home), *extra])
        return rc, out.getvalue()

    def test_fresh_install(self):
        with tempfile.TemporaryDirectory() as d:
            sd, home = Path(d, "sd"), Path(d, "home")
            fake_sd(sd)
            rc, out = self.run_install(sd, home)
            self.assertEqual(rc, 0, out)
            self.assertTrue((sd / "MODELS/model42.yml").read_bytes() == install_radio.MODEL_SRC.read_bytes())
            self.assertTrue((sd / "SCRIPTS/TOOLS/RideOnRF.lua").is_file())
            self.assertTrue((sd / "SCRIPTS/TELEMETRY/rideon.lua").is_file())
            self.assertIn('currModelFilename: "model42.yml"', (sd / "RADIO/radio.yml").read_text())
            self.assertEqual(len(list((home / "backups").glob("*.zip"))), 1)
            phrase = (home / "binding-phrase.txt").read_text().strip()
            self.assertTrue(phrase.startswith("rideon-") and len(phrase.split("-")) == 6)
            self.assertNotIn("WARNING", out)
            # Second run: same phrase, nothing changes.
            rc, out2 = self.run_install(sd, home, "--no-backup")
            self.assertIn(phrase, out2)
            self.assertIn("unchanged", out2)

    def test_reuses_existing_rideon_file_and_keeps_others(self):
        with tempfile.TemporaryDirectory() as d:
            sd, home = Path(d, "sd"), Path(d, "home")
            fake_sd(sd, extra_models=[("model42.yml", "Plane"), ("model07.yml", "RideOn")])
            rc, out = self.run_install(sd, home, "--no-backup")
            self.assertEqual(rc, 0)
            self.assertIn("Plane", (sd / "MODELS/model42.yml").read_text())
            self.assertIn("RideOn", (sd / "MODELS/model07.yml").read_text())
            self.assertIn("MODELS/model07.yml: updated", out)

    def test_warnings(self):
        with tempfile.TemporaryDirectory() as d:
            sd, home = Path(d, "sd"), Path(d, "home")
            fake_sd(sd, stick_mode=0, semver="2.9.4")
            (sd / "RideOn-radio.txt").write_text(
                "edgetx: 2.9.4 (tlitef4)\nelrs_tx: Jumper T-Lite 3.4.0\nelrs_rx: Jumper AION 4.1.0\n")
            rc, out = self.run_install(sd, home, "--dry-run")
            self.assertIn("too old", out)
            self.assertIn("stick mode 1", out)
            self.assertIn("different versions", out)
            self.assertFalse((sd / "MODELS/model42.yml").exists())  # dry run

    def test_not_an_sd_card(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out = self.run_install(Path(d), Path(d, "home"))
            self.assertEqual(rc, 2)


LOG_HEADER = "Date,Time,1RSS(dB),RQly(%),RSNR(dB),TPWR(mW),RxBt(V),Curr(A),FM,Ail,Ele,SA,SB,SC\n"


def log_rows(samples):
    """samples: (seconds, lq, rssi, batt, fm)"""
    out = LOG_HEADER
    for t, lq, rssi, batt, fm in samples:
        hh, rem = divmod(t, 3600)
        mm, ss = divmod(rem, 60)
        out += f"2026-11-07,{int(hh):02d}:{int(mm):02d}:{ss:06.3f},{rssi},{lq},9,25,{batt},1.0,{fm},0,0,-1024,-1024,1024\n"
    return out


class Logs(unittest.TestCase):
    def test_clean_run_passes(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d, "RideOn-2026-11-07.csv")
            p.write_text(log_rows([(36000 + i * 0.5, 100, -50, 24.0, "PARENT") for i in range(120)]))
            s = radio_logs.summarise(p)
            self.assertAlmostEqual(s.seconds, 59.5)
            self.assertEqual(s.dropouts, 0)
            self.assertIn("PASS", "\n".join(s.lines()))

    def test_dropout_low_lq_and_fault(self):
        rows = [(i * 0.5, 100, -60, 24.0, "PARENT") for i in range(20)]
        rows += [(10 + i * 0.5, 55, -105, 23.5, "PARENT") for i in range(4)]     # 2 s weak
        rows += [(12 + i * 0.5, 0, 0, 0, "") for i in range(3)]                    # 1.5 s lost
        rows += [(13.5 + i * 0.5, 90, -80, 21.2, "KILLED LINK") for i in range(10)]
        rows += [(18.5 + i * 0.5, 0, 0, 0, "") for i in range(4)]                  # car off at the end
        with tempfile.TemporaryDirectory() as d:
            p = Path(d, "RideOn-x.csv")
            p.write_text(log_rows(rows))
            s = radio_logs.summarise(p)
            self.assertEqual(s.dropouts, 1)
            self.assertAlmostEqual(s.longest_dropout_s, 1.5)
            self.assertEqual(s.rssi_min, -105)
            self.assertEqual(s.batt_min, 21.2)
            self.assertAlmostEqual(s.below_degraded_s, 2.0)
            self.assertIn("KILLED LINK", s.fm_seen)
            self.assertIn("CHECK", "\n".join(s.lines()))

    def test_installer_collects_logs(self):
        with tempfile.TemporaryDirectory() as d:
            sd, home = Path(d, "sd"), Path(d, "home")
            fake_sd(sd)
            (sd / "LOGS").mkdir()
            (sd / "LOGS/RideOn-2026-11-07.csv").write_text(log_rows([(i, 99, -40, 24.0, "LOCKED") for i in range(5)]))
            out = io.StringIO()
            with redirect_stdout(out):
                install_radio.main(["--sd", str(sd), "--home", str(home)])
            self.assertTrue((home / "logs/RideOn-2026-11-07.csv").is_file())
            self.assertIn("link quality: min 99 %", out.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
