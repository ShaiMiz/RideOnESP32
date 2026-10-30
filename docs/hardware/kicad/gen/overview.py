"""The system wiring overview: one A2 page with every assembly drawn and real lines joining them.

It is a drawing (graphic lines, boxes and text), not part of the netlist: the detailed sheets carry the
nets, and WIRE_LIST.md carries every single wire. Here a thin line is one wire and a thick blue, green
or grey line is a labelled bundle. Coordinates are mm on A2 (594 x 420), y down.
"""
import kicad as K

# line styles: (r, g, b, width mm)
ST = {
    "bat": (200, 0, 0, 0.9),       # battery + before K1, 10 AWG
    "mot": (230, 120, 0, 0.9),     # MOTOR_24V after K1, 10 AWG
    "gnd": (0, 0, 0, 0.9),         # power ground, 10 AWG
    "gndl": (0, 0, 0, 0.3),        # logic ground
    "lead": (140, 70, 20, 0.7),    # motor leads, 12 AWG
    "key": (240, 140, 0, 0.45),    # key-switched 24 V branches
    "coil": (240, 140, 0, 0.3),    # coil chain
    "5v": (220, 0, 0, 0.3),
    "3v3": (130, 0, 180, 0.3),
    "12v": (240, 140, 0, 0.4),
    "pwm": (0, 70, 200, 0.6),      # driver control bundle
    "i2c": (0, 140, 60, 0.6),
    "sig": (110, 110, 110, 0.5),   # switch and sensor signals
    "ana": (150, 90, 40, 0.5),     # analog (IS, pedal, battery sense)
    "spk": (60, 60, 60, 0.4),
}
FILL = {"box": (255, 255, 225), "car": (235, 245, 255), "ours": (235, 255, 235), "stud": (255, 235, 200),
        "plug": (255, 250, 240)}


class Drawing:
    def __init__(s):
        s.items, s.blocks, s.n = [], {}, 0

    def _u(s):
        s.n += 1
        return K.q(K.uid("overview", s.n))

    def text(s, x, y, t, size=1.27, bold=False, color=None, just="left top"):
        c = f" (color {color[0]} {color[1]} {color[2]} 1)" if color else ""
        b = " (bold yes)" if bold else ""
        s.items.append(f"(text {K.q(t)} (exclude_from_sim no) (at {K.fmt(x)} {K.fmt(y)} 0) "
                       f"(effects (font (size {size} {size}){b}{c}) (justify {just})) (uuid {s._u()}))")

    def rect(s, x, y, w, h, fill=None, width=0.25, dash=False, color=(120, 0, 0)):
        f = f"(fill (type color) (color {fill[0]} {fill[1]} {fill[2]} 1))" if fill else "(fill (type none))"
        s.items.append(f"(rectangle (start {K.fmt(x)} {K.fmt(y)}) (end {K.fmt(x + w)} {K.fmt(y + h)}) "
                       f"(stroke (width {width}) (type {'dash' if dash else 'solid'}) (color {color[0]} {color[1]} {color[2]} 1)) "
                       f"{f} (uuid {s._u()}))")

    def block(s, key, x, y, w, h, title, sub="", kind="box", ports=()):
        """ports: (name, side L/R/T/B, offset mm along that side)."""
        s.rect(x, y, w, h, FILL[kind])
        s.text(x + 1.2, y + 1.0, title, 1.4, bold=True)
        if sub:
            s.text(x + 1.2, y + 3.4, sub, 1.0)
        pts = {}
        for name, side, off in ports:
            pts[name] = {"L": (x, y + off), "R": (x + w, y + off), "T": (x + off, y), "B": (x + off, y + h)}[side]
            tx, ty = pts[name]
            if side == "L":
                s.text(tx + 0.8, ty, name, 0.9, just="left")
            elif side == "R":
                s.text(tx - 0.8, ty, name, 0.9, just="right")
            elif side == "T":
                s.text(tx, ty + 0.6, name, 0.9, just="top")
            else:
                s.text(tx, ty - 0.6, name, 0.9, just="bottom")
        s.blocks[key] = pts

    def p(s, ref):
        key, port = ref.split(".", 1)
        return s.blocks[key][port]

    def wire(s, a, b, style, via=(), label=None, lpos=0.5, dot_a=False, dot_b=False):
        """Orthogonal line from port a to port b; via = extra points; a bare number in via is an x to turn at."""
        r, g, bl, w = ST[style]
        pa, pb = (s.p(a) if isinstance(a, str) else a), (s.p(b) if isinstance(b, str) else b)
        pts = [pa]
        for v in via:
            if isinstance(v, (int, float)):       # turn at this x
                pts += [(v, pts[-1][1])]
            elif v[0] is None:                    # turn at this y
                pts += [(pts[-1][0], v[1])]
            else:
                pts.append(v)
        last = pts[-1]
        if last[0] != pb[0] and last[1] != pb[1]:
            pts.append((last[0], pb[1]))          # finish vertical, then horizontal
        pts.append(pb)
        xy = " ".join(f"(xy {K.fmt(x)} {K.fmt(y)})" for x, y in pts)
        s.items.append(f"(polyline (pts {xy}) (stroke (width {w}) (type solid) (color {r} {g} {bl} 1)) "
                       f"(fill (type none)) (uuid {s._u()}))")
        for on, pt in ((dot_a, pa), (dot_b, pb)):
            if on:
                s.items.append(f"(circle (center {K.fmt(pt[0])} {K.fmt(pt[1])}) (radius 0.6) "
                               f"(stroke (width 0) (type solid) (color {r} {g} {bl} 1)) (fill (type color) (color {r} {g} {bl} 1)) (uuid {s._u()}))")
        if label:
            # label on the longest segment
            segs = list(zip(pts, pts[1:]))
            (x0, y0), (x1, y1) = max(segs, key=lambda sg: abs(sg[0][0] - sg[1][0]) + abs(sg[0][1] - sg[1][1]))
            lx, ly = x0 + (x1 - x0) * lpos, y0 + (y1 - y0) * lpos
            if y0 == y1:
                s.text(lx, ly - 0.5, label, 0.9, color=(r, g, bl), just="bottom")
            else:
                s.items.append(f"(text {K.q(label)} (exclude_from_sim no) (at {K.fmt(lx - 0.5)} {K.fmt(ly)} 90) "
                               f"(effects (font (size 0.9 0.9) (color {r} {g} {bl} 1)) (justify bottom)) (uuid {s._u()}))")


def plug(d, key, x, y, title, pins, side="L"):
    """A box-edge plug straddling the box wall at x; pins listed top to bottom, ports on both faces."""
    h = 3.0 + 2.6 * len(pins)
    d.rect(x - 4, y, 8, h, FILL["plug"], width=0.35)
    d.text(x, y - 0.4, title, 1.1, bold=True, just="bottom")
    pts = {}
    for i, pn in enumerate(pins):
        yy = y + 2.6 + 2.6 * i
        pts["o" + pn] = (x - 4, yy) if side == "L" else (x + 4, yy)   # outside face (car)
        pts["i" + pn] = (x + 4, yy) if side == "L" else (x - 4, yy)   # inside face (box)
        d.text(x, yy, pn, 0.8, just="")
    d.blocks[key] = pts


def build():
    d = Drawing()
    # ---------------------------------------------------------------- frame and zones
    d.text(15, 14, "System wiring overview: everything connected", 3.0, bold=True)
    d.text(15, 19.5, "Each thin line is one wire; thick blue, green and grey lines are labelled bundles. "
           "Wire numbers, gauges and every single conductor are in WIRE_LIST.md; pins in CONNECTORS.md. "
           "Drawing only: the nets live on sheets 3-9.", 1.3)
    d.rect(150, 30, 290, 342, None, width=0.5, dash=True, color=(0, 0, 0))
    d.text(152, 31.5, "THE BOX (in the stock controller's place under the seat)", 1.6, bold=True)
    d.text(15, 31.5, "CAR SIDE (stock parts and our off-box parts)", 1.6, bold=True)
    d.text(455, 31.5, "MOTORS (stock)", 1.6, bold=True)

    # ---------------------------------------------------------------- car side, left
    d.block("BT1", 15, 40, 26, 18, "BT1 battery", "24 V, chemistry TBD (#13)", "car", [("+", "R", 5), ("-", "R", 14)])
    d.block("F1", 52, 41, 16, 8, "F1 40 A", "≤15 cm from +", "car", [("a", "L", 4), ("b", "R", 4)])
    d.block("SW1", 95, 80, 26, 12, "SW1 key", "stock dash", "car", [("1", "R", 4), ("2", "R", 9)])
    d.block("S2", 70, 112, 51, 26, "S2 E-STOP", "latching, 2 NC in series + 1 NO", "ours",
            [("NC in", "R", 6), ("NC out", "R", 11), ("NO", "R", 17), ("NO gnd", "R", 22)])
    d.block("PED", 95, 158, 26, 15, "Pedal", "type TBD (#16)", "car", [("V+", "R", 5), ("SIG", "R", 9), ("GND", "R", 13)])
    d.block("SH", 95, 188, 26, 13, "Shifter F/R", "stock, signal only", "car", [("F", "R", 5), ("R", "R", 8), ("GND", "R", 11)])
    d.block("HL", 80, 218, 41, 15, "Horn button, lights", "stock (TBD #17)", "car",
            [("L+", "R", 5), ("L-", "R", 8), ("HORN", "R", 11), ("GND", "R", 14)])
    d.block("SEN", 15, 245, 106, 30, "Sensors", "H1/H2 A3144 at the wheels, U25 AS5600 on the steering column, SW5 seat", "ours",
            [("5V", "R", 7), ("GND", "R", 10), ("WS_L", "R", 13), ("WS_R", "R", 16), ("3V3", "R", 19), ("SCL", "R", 22),
             ("SDA", "R", 25), ("SEAT", "R", 28)])
    d.block("DASH", 80, 288, 41, 20, "Dash", "U27 OLED, LS1 alert speaker", "ours",
            [("3V3", "R", 5), ("GND", "R", 8), ("SCL", "R", 11), ("SDA", "R", 14), ("SPK+", "R", 17), ("SPK-", "R", 19)])
    d.block("MUS", 80, 322, 41, 14, "Music speakers", "LS2, LS3 (+ tweeters)", "ours",
            [("L+", "R", 5), ("L-", "R", 7), ("R+", "R", 9), ("R-", "R", 11)])

    # ---------------------------------------------------------------- box-edge plugs (left wall x=150, right wall x=440)
    plug(d, "J1", 150, 42, "J1 BATT", ["1", "2"])
    plug(d, "J2", 150, 78, "J2 KEY", ["1", "2"])
    plug(d, "J9", 150, 112, "J9 E-STOP", ["1", "2", "3", "4"])
    plug(d, "J3", 150, 156, "J3 PEDAL", ["1", "2", "3"])
    plug(d, "J4", 150, 186, "J4 SHIFT", ["1", "2", "3"])
    plug(d, "J8", 150, 216, "J8 ACC", ["1", "2", "3", "4"])
    plug(d, "J10", 150, 244, "J10 SENS", ["1", "2", "3", "4", "5", "6", "7", "8"])
    plug(d, "J11", 150, 287, "J11 DASH", ["1", "2", "3", "4", "5", "6"])
    plug(d, "J12", 150, 321, "J12 MUSIC", ["1", "2", "3", "4"])
    plug(d, "J6", 440, 72, "J6 FRONT", ["1", "2", "3", "4"], side="R")
    plug(d, "J7", 440, 172, "J7 REAR", ["1", "2", "3", "4"], side="R")
    plug(d, "J5", 440, 258, "J5 STEER", ["1", "2"], side="R")

    # ---------------------------------------------------------------- inside the box
    d.block("K1", 168, 40, 34, 22, "K1 contactor", "125 A, 24 V coil", "box",
            [("A", "L", 6), ("B", "T", 28), ("C+", "B", 8), ("C-", "B", 24)])
    d.block("F2", 168, 70, 22, 8, "F2 15 A", "inline", "box", [("a", "L", 4), ("b", "R", 4)])
    d.block("FB", 168, 96, 34, 56, "FB1 fuse block", "bus = KEY_24V", "box",
            [("IN", "L", 6), ("F3", "R", 10), ("F4", "R", 18), ("F5", "R", 26), ("F6", "R", 34), ("F7", "R", 42), ("F8", "R", 50)])
    d.block("U1", 222, 110, 34, 10, "U1 LM2596 5 V", "5V_LOGIC", "box", [("in", "L", 6.5), ("out", "R", 6.5), ("gnd", "B", 17)])
    d.block("U3", 222, 128, 34, 10, "U3 buck 5 V 5 A", "5V_ACC", "box", [("in", "L", 6.5), ("out", "R", 6.5), ("gnd", "B", 17)])
    d.block("U4", 222, 146, 34, 10, "U4 buck 12 V", "12V_ACC", "box", [("in", "L", 6.5), ("out", "R", 6.5), ("gnd", "B", 17)])
    d.block("U2", 222, 92, 34, 10, "U2 AMS1117 3.3 V", "3V3_SENSE", "box", [("in", "R", 6.5), ("out", "L", 6.5)])
    d.block("U20", 268, 100, 40, 78, "U20 ESP32", "pins from pins.h", "box",
            [("VIN/GND", "L", 8), ("SCL/SDA", "B", 10), ("RX/TX", "B", 30), ("analog", "L", 30), ("GPIO12", "L", 40),
             ("FL", "R", 10), ("FR", "R", 25), ("RL", "R", 40), ("RR", "R", 55), ("ST", "R", 70), ("TX0", "R", 76)])
    d.block("U21", 268, 190, 40, 12, "U21 ELRS receiver", "CRSF, antenna outside", "box", [("5V", "L", 6), ("UART", "T", 30)])
    d.block("A1", 196, 172, 62, 50, "A1 interface board", "Q1, IS RC + clamps, pedal and hall inputs, divider, buses", "box",
            [("Q1 drain", "L", 8), ("divider", "T", 8), ("5V in", "T", 50), ("3V3 in", "T", 58), ("GND", "B", 56),
             ("to ESP32", "R", 10), ("5V/3V3 out", "R", 22), ("IS in", "R", 34), ("IS out", "B", 20),
             ("pedal", "L", 22), ("halls", "L", 32), ("buses out", "L", 44)])
    yd = [44, 92, 140, 188, 236]
    for key, y, name in zip(["U10", "U11", "U12", "U13", "U14"], yd, ["FL", "FR", "RL", "RR", "STEER"]):
        d.block(key, 330, y, 42, 36, f"{key} IBT-2 {name}", "BTS7960, 470 µF on B+/B−", "box",
                [("ctrl", "L", 8), ("IS", "L", 26), ("B+", "R", 6), ("B-", "R", 12), ("M+", "R", 22), ("M-", "R", 28)])
    d.rect(392, 40, 5, 240, FILL["stud"])
    d.text(394.5, 39.5, "TB2 MOTOR_24V", 1.0, bold=True, just="bottom")
    d.rect(406, 40, 5, 318, FILL["stud"])
    d.text(410, 360, "TB1 GND star", 1.0, bold=True, just="top")
    for i, name in enumerate(["U22 ADS1115 0x48", "U23 ADS1115 0x49", "U24 PCF8574 0x20", "U26 MPU-6050", "U28 PCA9685"]):
        x = 162 + i * 33
        d.block(f"I{i}", x, 330, 29, 14, name.split(" ", 1)[0], name.split(" ", 1)[1], "box",
                [("in", "L", 8), ("out", "R", 8)] + {0: [("IS x5", "T", 15)], 2: [("P0-P7", "B", 15)], 4: [("PWM", "B", 15)]}.get(i, []))
    d.block("U29", 330, 296, 34, 12, "U29 MOSFET board", "low-side switches", "box",
            [("in", "L", 6), ("fan", "R", 4), ("lights", "R", 9)])
    d.block("FAN", 330, 282, 34, 9, "FAN1/2 24 V", "box fans", "box", [("+", "L", 4), ("-", "R", 4)])
    d.block("U30", 330, 316, 34, 10, "U30 DFPlayer", "car sounds", "box", [("in", "L", 3), ("pwr", "L", 7), ("spk", "B", 17)])
    d.block("F9", 268, 208, 22, 10, "F9 / F10", "3 A lights, 10 A amp", "box", [("in", "L", 5.5), ("lights", "R", 6), ("amp", "R", 8.5)])
    d.block("U33", 330, 330, 34, 10, "U33 Bluetooth log", "HM-10 BLE, listen-only", "box", [("in", "L", 5)])
    d.block("U32", 330, 346, 34, 12, "U32 music amp", "ZK-502T, its own Bluetooth", "box", [("in", "L", 6), ("out", "B", 10)])

    # ---------------------------------------------------------------- power path
    d.wire("BT1.+", "F1.a", "bat", label="W001 10 AWG")
    d.wire("F1.b", "J1.o1", "bat", via=[(80, 45), (80, 44.6)])
    d.wire("J1.i1", "K1.A", "bat", label="10 AWG")
    d.wire("BT1.-", "J1.o2", "gnd", via=[(90, 54), (90, 47.2)])
    d.wire("J1.i2", (408.5, 366), "gnd", via=[(158, 47.2), (158, 366)], label="J1.2 → TB1 GND star, 10 AWG", lpos=0.4)
    d.wire("K1.B", (394.5, 40), "mot", via=[(None, 36), (394.5, 36)], label="K1 → TB2, 10 AWG")
    for key in ["U10", "U11", "U12", "U13", "U14"]:
        bp, bm = d.p(key + ".B+"), d.p(key + ".B-")
        d.wire(key + ".B+", (392, bp[1]), "mot")
        d.wire(key + ".B-", (406, bm[1]), "gnd")
    # motor leads to the plugs (crossing the stud bars, not joined)
    for key, j, a, b in [("U10", "J6", "1", "2"), ("U11", "J6", "3", "4"), ("U12", "J7", "1", "2"), ("U13", "J7", "3", "4"),
                         ("U14", "J5", "1", "2")]:
        for port, pin in (("M+", a), ("M-", b)):
            d.wire(f"{key}.{port}", f"{j}.i{pin}", "lead", via=[420])
    # motors
    for i, (j, pins, name) in enumerate([("J6", ("1", "2"), "M1 FL"), ("J6", ("3", "4"), "M2 FR"), ("J7", ("1", "2"), "M3 RL"),
                                         ("J7", ("3", "4"), "M4 RR"), ("J5", ("1", "2"), "M5 steering")]):
        y = [60, 86, 160, 186, 254][i]
        d.block(f"M{i}", 470, y, 34, 14, name, "C 100 nF on the tabs", "car", [("+", "L", 5), ("-", "L", 9)])
        d.wire(f"{j}.o{pins[0]}", f"M{i}.+", "lead", via=[458])
        d.wire(f"{j}.o{pins[1]}", f"M{i}.-", "lead", via=[461])
    d.text(470, 210, "J6/J7: one pair per motor. If the stock\nharness pairs motors, a reversible\nsplitter at the motor plugs (#33).", 1.0)

    # ---------------------------------------------------------------- key and coil chain
    d.wire("K1.A", "F2.a", "bat", via=[(164, 46), (164, 74)])
    d.wire("F2.b", "J2.i1", "key", via=[(194, 74), (194, 84), (160, 84), (160, 80.6)], label="")
    d.wire("J2.o1", "SW1.1", "key", via=[(130, 80.6), (130, 84)])
    d.wire("SW1.2", "J2.o2", "key", via=[(134, 89), (134, 83.2)])
    d.wire("J2.i2", "FB.IN", "key", via=[(162, 83.2), (162, 102)], label="KEY_24V")
    d.wire("FB.F3", (214, 106), "coil", via=[(214, 106), (214, 66), (161, 66), (161, 114.6)])
    d.wire((161, 114.6), "J9.i1", "coil")
    d.wire("J9.o1", "S2.NC in", "coil", via=[(135, 114.6), (135, 118)])
    d.wire("S2.NC out", "J9.o2", "coil", via=[(137, 123), (137, 117.2)])
    d.wire("J9.i2", "K1.C+", "coil", via=[(163, 117.2), (163, 64), (176, 64)], label="coil +")
    d.wire("K1.C-", "A1.Q1 drain", "coil", via=[(192, 64), (192, 180)], label="coil − → Q1")
    d.wire("S2.NO", "J9.o3", "sig", via=[(139, 129), (139, 119.8)])
    d.wire("S2.NO gnd", "J9.o4", "gndl", via=[(141, 134), (141, 122.4)])
    d.wire("U20.GPIO12", "A1.to ESP32", "pwm", via=[(262, 140), (262, 182)], label="CONTACTOR_HOLD, sense, analog")
    # fuse block branches
    d.wire("FB.F4", "U1.in", "key", via=[216])
    d.wire("FB.F5", "U3.in", "key", via=[216])
    d.wire("FB.F6", "U4.in", "key", via=[216])
    d.wire("FB.F7", "FAN.+", "key", via=[(204, 138), (204, 166), (322, 166), (322, 286)], label="F7 → fans")
    d.wire("FB.F8", "A1.divider", "key", via=[(206, 146), (206, 168), (204, 168)], label="F8")
    # rails
    d.wire("U1.out", "A1.5V in", "5v", via=[(262, 116.5), (262, 166), (246, 166)], label="5V")
    d.wire("U2.out", "A1.3V3 in", "3v3", via=[(218, 98.5), (218, 160), (254, 160)], label="3V3")
    d.wire("A1.5V/3V3 out", "U2.in", "5v", via=[(264, 194), (264, 98.5)])
    d.wire("A1.5V/3V3 out", "U20.VIN/GND", "5v", via=[(264, 194), (264, 108)])
    d.wire("A1.5V/3V3 out", "U21.5V", "5v", via=[(264, 194), (264, 196)])
    d.wire("U3.out", "U30.pwr", "5v", via=[(260, 134.5), (260, 164), (318, 164), (318, 323)], label="5V_ACC")
    d.wire("U4.out", "F9.in", "12v", via=[(258, 152.5), (258, 213.5)])
    d.wire("F9.amp", "U32.in", "12v", via=[(314, 216.5), (314, 352)], label="12 V amp")
    d.wire("F9.lights", "J8.i1", "12v", via=[(292, 214), (292, 228), (166, 228), (166, 218.6)], label="12 V lights")
    d.wire("U29.lights", "J8.i2", "sig", via=[(368, 305), (368, 230), (168, 230), (168, 221.2)], label="lights low side")
    d.wire("U29.fan", "FAN.-", "sig", via=[(366, 300), (366, 286)])
    # grounds
    for k in ["U1", "U3", "U4"]:
        g = d.p(k + ".gnd")
        d.wire(k + ".gnd", (406, g[1] + 3), "gndl", via=[(g[0], g[1] + 3)])
    d.wire("A1.GND", (406, 226), "gndl", via=[(252, 226)], label="A1 ground bus → TB1")

    # ---------------------------------------------------------------- control and sensing
    for key, port in [("U10", "FL"), ("U11", "FR"), ("U12", "RL"), ("U13", "RR"), ("U14", "ST")]:
        a, b = d.p(f"U20.{port}"), d.p(f"{key}.ctrl")
        d.wire(f"U20.{port}", f"{key}.ctrl", "pwm", via=[318], label="RPWM LPWM EN 3V3 GND" if key == "U10" else None)
    for key in ["U10", "U11", "U12", "U13", "U14"]:
        d.wire(f"{key}.IS", (324, d.p(key + ".IS")[1]), "ana")
    d.wire((324, 70), "A1.IS in", "ana", via=[(324, 206)], label="IS ×5")
    d.wire((324, 70), (324, 262), "ana")
    d.wire("A1.IS out", "I0.IS x5", "ana", via=[(216, 326), (177, 326)], label="IS filtered")
    d.wire("U20.SCL/SDA", "I0.in", "i2c", via=[(278, 184), (312, 184), (312, 322), (160, 322), (160, 338)], label="I2C bus", lpos=0.3)
    for i in range(4):
        d.wire(f"I{i}.out", f"I{i + 1}.in", "i2c")
    d.wire("I4.out", "J10.i6", "i2c", via=[(326, 338), (326, 278), (170, 278), (170, 262.2)], label="I2C to J10 / J11")
    d.wire((170, 278), "J11.i3", "i2c", via=[(170, 295)])
    d.wire("U20.RX/TX", "U21.UART", "sig", via=[(298, 186)])
    d.wire("A1.pedal", "J3.i2", "ana", via=[(174, 194), (174, 163.8)])
    d.wire("A1.halls", "J10.i3", "sig", via=[(176, 204), (176, 252.4)])
    d.wire("A1.buses out", "J10.i1", "5v", via=[(178, 216), (178, 246.6)], label="5V 3V3 GND")
    d.wire("I2.P0-P7", "J4.i1", "sig", via=[(None, 348), (156, 348), (156, 188.6)], label="switch lines to U24: shifter, horn, E-stop sense, seat", lpos=0.35)
    for j, pin in [("J4", "2"), ("J8", "3"), ("J9", "3"), ("J10", "8")]:
        y = d.p(f"{j}.i{pin}")[1]
        d.wire(f"{j}.i{pin}", (156, y), "sig", dot_b=True)
    d.wire("I4.PWM", "U29.in", "pwm", via=[(None, 352), (326, 352), (326, 302)], label="PWM")
    d.wire("A1.to ESP32", "U30.in", "sig", via=[(266, 182), (266, 319)], label="SOUND_TX")
    d.wire("U20.TX0", "U33.in", "sig", via=[(314, 176), (314, 335)], label="TX0 log →")
    d.wire("U30.spk", "J11.i5", "spk", via=[(347, 362), (172, 362), (172, 300)], label="alert speaker")
    d.wire("U32.out", "J12.i1", "spk", via=[(340, 368), (174, 368), (174, 323.6)], label="music L/R")

    # ---------------------------------------------------------------- car-side runs to the plugs
    for j, blk, pairs in [("J3", "PED", [("1", "V+"), ("2", "SIG"), ("3", "GND")]),
                          ("J4", "SH", [("1", "F"), ("2", "R"), ("3", "GND")]),
                          ("J8", "HL", [("1", "L+"), ("2", "L-"), ("3", "HORN"), ("4", "GND")]),
                          ("J10", "SEN", [("1", "5V"), ("2", "GND"), ("3", "WS_L"), ("4", "WS_R"), ("5", "3V3"), ("6", "SCL"),
                                          ("7", "SDA"), ("8", "SEAT")]),
                          ("J11", "DASH", [("1", "3V3"), ("2", "GND"), ("3", "SCL"), ("4", "SDA"), ("5", "SPK+"), ("6", "SPK-")]),
                          ("J12", "MUS", [("1", "L+"), ("2", "L-"), ("3", "R+"), ("4", "R-")])]:
        for k, (pin, port) in enumerate(pairs):
            d.wire(f"{blk}.{port}", f"{j}.o{pin}", "sig", via=[128 + k * 1.2])

    # ---------------------------------------------------------------- legend
    lx, ly = 455, 280
    d.text(lx, ly, "Legend", 1.6, bold=True)
    for i, (st, txt) in enumerate([("bat", "Battery + before K1, 10 AWG"), ("mot", "MOTOR_24V after K1, 10 AWG"),
                                   ("gnd", "Power ground to TB1, 10 AWG"), ("lead", "Motor leads, 12 AWG"),
                                   ("key", "Key-switched 24 V branches (F2-F8)"), ("coil", "K1 coil chain (F3 1 A)"),
                                   ("5v", "5 V"), ("3v3", "3.3 V"), ("12v", "12 V accessories"),
                                   ("pwm", "Driver control / PWM bundles"), ("i2c", "I2C bus"), ("ana", "Analog: IS, pedal"),
                                   ("sig", "Switches, sensors, serial"), ("gndl", "Logic ground")]):
        y = ly + 4 + i * 3.2
        d.wire((lx, y), (lx + 10, y), st)
        d.text(lx + 12, y, txt, 1.0, just="left")
    return d.items
