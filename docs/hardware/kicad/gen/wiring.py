"""Physical wire runs: which terminal each wire goes from and to, its gauge, colour and the fuse that protects it.

generate.py checks every run against the schematic: both ends must be on the same net, and every pin
must be reached by a run, sit on the interface board A1, or be listed in DIRECT with the reason.

Endpoints are REF.PIN as in the schematic. "A1:GND", "A1:3V3" and "A1:5V" are the bus strips on the
interface board A1 (a screw-terminal proto board in the box that holds the small parts: sense
networks, pull-ups, Q1, the battery divider).
"""

BUS = {"A1:GND": "GND", "A1:3V3": "3V3_SENSE", "A1:5V": "5V_LOGIC"}

# ---- colour code (22 AWG silicone set colours; 10 and 12 AWG are red and black only, so the
# 24 V side uses coloured heat-shrink bands at both ends)
COLOURS = [
    ("RD", "red", "battery + and motor + side (10/12 AWG); 5 V rails (22 AWG)"),
    ("RD/YE", "red, yellow band", "MOTOR_24V: switched by K1"),
    ("BK", "black", "ground, motor − side"),
    ("OG", "orange (or red + orange band)", "key-switched 24 V branches after F2-F8, 12 V branches"),
    ("VT", "violet", "3V3_SENSE"),
    ("YE", "yellow", "kill lines: DRIVE_EN, STEER_EN, CONTACTOR_HOLD"),
    ("BU", "blue", "motor PWM (RPWM and LPWM)"),
    ("GY", "grey", "I2C SCL"),
    ("GN", "green", "I2C SDA"),
    ("BN", "brown", "analog: IS sense, pedal, battery sense"),
    ("WH", "white", "digital inputs, UART, 1-Wire, switch lines"),
    ("BK/WH", "black, white band", "low-side switched returns (lights, fans)"),
]


def net_class(net):
    """Default gauge (AWG), colour and protecting fuse for a net."""
    if net == "BATT_RAW":
        return 10, "RD", "none: keep ≤ 15 cm, battery + to F1"
    if net == "BATT_FUSED":
        return 10, "RD", "F1 40 A"
    if net == "MOTOR_24V":
        return 10, "RD/YE", "F1 40 A (after K1)"
    if net.endswith("_DRV_POS"):
        return 12, "RD", "F1 40 A + driver current limit"
    if net.endswith("_M_POS"):
        return 12, "RD", "F1 40 A" if net == "STEER_M_POS" else "F11-F14 15 A (one per motor)"
    if net.endswith("_M_NEG"):
        return 12, "BK", "F1 40 A + driver current limit"
    if net in ("KEY_FEED", "KEY_24V"):
        return 14, "OG", "F2 15 A"
    if net in ("COIL_FEED", "ESTOP_MID", "COIL_24V", "COIL_LOW"):
        return 20, "OG", "F3 1 A"
    if net == "U1_IN":
        return 20, "OG", "F4 2 A"
    if net == "U3_IN":
        return 20, "OG", "F5 3 A"
    if net == "U4_IN":
        return 16, "OG", "F6 10 A"
    if net in ("FAN_24V",):
        return 20, "OG", "F7 1 A"
    if net == "FAN_LOW":
        return 20, "BK/WH", "F7 1 A"
    if net == "BATT_DIV":
        return 22, "OG", "F8 1 A"
    if net == "12V_ACC":
        return 14, "OG", "F6 10 A (24 V side)"
    if net == "LIGHTS_12V":
        return 20, "OG", "F9 3 A"
    if net == "LIGHTS_SW":
        return 20, "BK/WH", "F9 3 A"
    if net == "AMP_12V":
        return 16, "OG", "F10 10 A"
    if net == "5V_ACC":
        return 20, "RD", "U3 current limit, F5"
    if net == "5V_LOGIC":
        return 22, "RD", "F4 2 A (24 V side)"
    if net == "3V3_SENSE":
        return 22, "VT", "U2 current limit"
    if net == "GND":
        return 22, "BK", "-"
    if net in ("DRIVE_EN", "STEER_EN", "CONTACTOR_HOLD"):
        return 22, "YE", "-"
    if net.endswith("PWM"):
        return 22, "BU", "-"
    if net == "I2C_SCL":
        return 22, "GY", "-"
    if net == "I2C_SDA":
        return 22, "GN", "-"
    if net.startswith("IS_") or net in ("PEDAL", "PEDAL_IN", "PEDAL_VCC", "BATTERY_SENSE", "BUS_SENSE"):
        return 22, "BN", "-"
    if net.startswith("SPK_"):
        return 18, "RD" if net.endswith("_P") else "BK", "amp output"
    return 22, "WH", "-"


# Runs: (from, to, route, note[, gauge][, colour]). Route: box = inside the box; harness = our new
# off-box harness; stock = the car's own harness (not touched); adapter = new lead at the battery.
RUNS = []


def run(a, b, route="box", note="", gauge=None, colour=None):
    RUNS.append(dict(a=a, b=b, route=route, note=note, gauge=gauge, colour=colour))


# ---- main power
run("BT1.1", "F1.1", "adapter", "Ring lug on battery +; ≤ 15 cm, unfused")
run("F1.2", "J1.1", "adapter", "Or the stock battery lead if it already has a 40 A fuse this close")
run("BT1.2", "J1.2", "stock", "Battery −", gauge=10)
run("J1.1", "K1.A", "box", "M6/M8 ring lugs")
run("J1.2", "TB1.1", "box", "Battery − to the ground star", gauge=10)
run("K1.B", "TB2.1", "box")
for i, u in enumerate(["U10", "U11", "U12", "U13", "U14"]):
    run(f"TB2.{i + 2}", f"{u}.B+", "box", "470 µF cap on the same terminal")
    run(f"{u}.B-", f"TB1.{i + 2}", "box", "Run beside the B+ wire", gauge=10)
run("K1.A", "F2.1", "box", "The holder's own short 12 AWG lead, ring lug on K1 stud A", gauge=12)
run("F2.2", "J2.1", "box")
run("J2.1", "SW1.1", "stock", "Key switch")
run("SW1.2", "J2.2", "stock")
run("J2.2", "F3.1", "box", "Fuse block bus input; the bus feeds F3-F8")

# ---- coil chain
run("F3.2", "J9.1", "box")
run("J9.1", "S2.11", "harness", "E-stop cable, 4-core")
run("S2.12", "S2.21", "harness", "Jumper inside the E-stop: NC1 and NC2 in series")
run("S2.22", "J9.2", "harness")
run("J9.2", "K1.C+", "box", "D1 soldered across the coil tabs")
run("K1.C-", "Q1.D", "box", "To A1")
run("S2.33", "J9.3", "harness")
run("S2.34", "J9.4", "harness")
run("J9.3", "U24.P3", "box")
run("J9.4", "A1:GND", "box")
run("U20.L12", "R1.1", "box", "Twist with a GND wire")

# ---- fuse block branches
run("F4.2", "U1.IN+", "box")
run("F5.2", "U3.IN+", "box")
run("F6.2", "U4.IN+", "box")
run("F7.2", "FAN1.1", "box")
run("FAN1.1", "FAN2.1", "box")
run("FAN1.2", "U29.OUT2", "box")
run("FAN2.2", "U29.OUT2", "box")
run("F8.2", "R3.1", "box", "To A1")
run("U1.IN-", "TB1.7", "box", "IN− and OUT− are joined inside the module", gauge=20)
run("U3.IN-", "TB1.8", "box", "", gauge=20)
run("U4.IN-", "TB1.9", "box", "", gauge=16)
run("A1:GND", "TB1.10", "box", "A1 ground bus: the only logic-ground wire to the star", gauge=18)
run("U29.PGND", "TB1.11", "box", "", gauge=20)
run("U32.VIN-", "TB1.12", "box", "", gauge=16)

# ---- rails
run("U1.OUT+", "A1:5V", "box", "", gauge=20)
run("A1:5V", "U20.L15", "box", "ESP32 VIN: 5 V, never 3.3 V")
run("A1:5V", "U21.5V", "box")
run("A1:5V", "U2.VIN", "box")
run("A1:5V", "J10.1", "box", "Hall sensor supply")
run("U2.VOUT", "A1:3V3", "box")
run("U2.GND", "A1:GND", "box")
run("U20.L14", "A1:GND", "box", "R14 GND is joined on the board")
run("U21.GND", "A1:GND", "box")
run("U3.OUT+", "U30.1", "box", "DFPlayer VCC")
run("U30.7", "A1:GND", "box")
run("U4.OUT+", "F9.1", "box")
run("F9.1", "F10.1", "box", "Second inline holder on the same 12 V lead")
run("F10.2", "U32.VIN+", "box")
run("F9.2", "J8.1", "box", "Lights supply")
run("U29.OUT1", "J8.2", "box", "Lights low side")

# ---- drivers: one 8-way control plug per IBT-2
ESP = {"FL": ("L8", "L9"), "FR": ("L10", "L11"), "RL": ("L6", "L7"), "RR": ("L13", "R11"), "STEER": ("R7", "R6")}
DRV = {"FL": "U10", "FR": "U11", "RL": "U12", "RR": "U13", "STEER": "U14"}
ISR = {"FL": "R101", "FR": "R102", "RL": "R103", "RR": "R104", "STEER": "R105"}
prev = "U20.R5"
for c in ["FL", "FR", "RL", "RR"]:
    u = DRV[c]
    run(f"U20.{ESP[c][0]}", f"{u}.1", "box", "Control cable: twist PWM and EN with GND")
    run(f"U20.{ESP[c][1]}", f"{u}.2", "box")
    run(prev, f"{u}.3", "box", "DRIVE_EN daisy chain, ESP32 → FL → FR → RL → RR")
    prev = f"{u}.3"
for c in ["FL", "FR", "RL", "RR", "STEER"]:
    u = DRV[c]
    if c == "STEER":
        run(f"U20.{ESP[c][0]}", f"{u}.1", "box", "Control cable: twist PWM and EN with GND")
        run(f"U20.{ESP[c][1]}", f"{u}.2", "box")
        run("U20.R12", f"{u}.3", "box", "STEER_EN")
    run(f"{u}.3", f"{u}.4", "box", "Jumper in the plug: R_EN = L_EN; the 10k pull-down sits in the plug too")
    run(f"{u}.5", f"{u}.6", "box", "Jumper in the plug: R_IS = L_IS")
    run(f"{u}.5", f"{ISR[c]}.1", "box", "IS to A1")
    run("A1:3V3", f"{u}.7", "box", "Logic VCC")
    run("A1:GND", f"{u}.8", "box", "Signal reference; B− is the power ground")
    jpos = f"J{6 if c in ('FL', 'FR') else 7 if c != 'STEER' else 5}.{ {'FL': 1, 'FR': 3, 'RL': 1, 'RR': 3, 'STEER': 1}[c] }"
    if c == "STEER":
        run(f"{u}.M+", jpos, "box")
    else:
        fuse = {"FL": "F11", "FR": "F12", "RL": "F13", "RR": "F14"}[c]
        run(f"{u}.M+", f"{fuse}.1", "box", "Motor + to its inline fuse")
        run(f"{fuse}.2", jpos, "box")
    run(f"{u}.M-", f"J{6 if c in ('FL', 'FR') else 7 if c != 'STEER' else 5}.{ {'FL': 2, 'FR': 4, 'RL': 2, 'RR': 4, 'STEER': 2}[c] }", "box")

# ---- motors (car side)
for j, pins in (("J6", [("1", "M1.1"), ("2", "M1.2"), ("3", "M2.1"), ("4", "M2.2")]),
                ("J7", [("1", "M3.1"), ("2", "M3.2"), ("3", "M4.1"), ("4", "M4.2")]),
                ("J5", [("1", "M5.1"), ("2", "M5.2")])):
    for p, m in pins:
        run(f"{j}.{p}", m, "stock", "Stock motor wiring (or the splitter, #33)")

# ---- ESP32 signals to A1 and modules
run("U20.L2", "R20.2", "box", "PEDAL to A1")
run("U20.L3", "R3.2", "box", "BATTERY_SENSE to A1")
run("U20.L4", "R23.2", "box", "WHEEL_SPEED_L to A1")
run("U20.L5", "R24.2", "box", "WHEEL_SPEED_R to A1")
run("U20.R13", "R27.2", "box", "ONEWIRE_TEMP to A1")
run("U20.R8", "R28.1", "box", "SOUND_TX to A1")
run("R28.2", "U30.2", "box", "DF_RX")
run("U20.R3", "R29.1", "box", "LOG_TX (TX0) to A1")
run("R29.2", "U33.RXD", "box", "BT_RXD. U33 TXD stays unconnected: listen-only")
run("A1:5V", "U33.VCC", "box", "Bluetooth module VCC")
run("U33.GND", "A1:GND", "box")
run("U20.R10", "U21.TX", "box", "CRSF: receiver TX → ESP32 RX")
run("U20.R9", "U21.RX", "box", "CRSF: ESP32 TX → receiver RX")
run("R1.2", "Q1.G", "box", "On A1 (listed because Q1 is a TO-220 on flying leads if not on the board)")
run("R101.2", "U22.A0", "box")
run("R102.2", "U22.A1", "box")
run("R103.2", "U22.A2", "box")
run("R104.2", "U22.A3", "box")
run("R105.2", "U23.A0", "box")
run("R5.2", "R6.1", "box", "BUS_SENSE from the R5 lead at TB2 to A1")
run("R6.1", "U23.A2", "box")
run("R7.2", "A1:GND", "box", "Bleed return", gauge=20)
run("A1:3V3", "R22.1", "box", "On A1")
run("R22.2", "J3.1", "box", "Pedal supply")
run("J3.2", "R20.1", "box", "Pedal signal")
run("J3.3", "A1:GND", "box")
run("J3.1", "PED1.V+", "stock")
run("J3.2", "PED1.SIG", "stock")
run("J3.3", "PED1.GND", "stock")
run("U31.DQ", "R27.2", "box", "DS18B20 yellow")
run("U31.VDD", "A1:3V3", "box", "DS18B20 red")
run("U31.GND", "A1:GND", "box", "DS18B20 black")

# ---- I2C bus: one 4-wire daisy chain through the in-box devices, then off-box
chain = ["U22", "U23", "U24", "U26", "U28"]
pin = {"U22": ("SCL", "SDA", "VDD", "GND"), "U23": ("SCL", "SDA", "VDD", "GND"), "U24": ("SCL", "SDA", "VCC", "GND"),
       "U26": ("SCL", "SDA", "VCC", "GND"), "U28": ("SCL", "SDA", "VCC", "GND")}
run("U20.R2", "U22.SCL", "box", "I2C bus start; keep SCL and SDA beside GND")
run("U20.R1", "U22.SDA", "box")
run("A1:3V3", "U22.VDD", "box")
run("A1:GND", "U22.GND", "box")
for a, b in zip(chain, chain[1:]):
    for k in range(4):
        run(f"{a}.{pin[a][k]}", f"{b}.{pin[b][k]}", "box", "I2C chain")
run("U22.ADDR", "U22.GND", "box", "Address 0x48")
run("U23.ADDR", "U23.VDD", "box", "Address 0x49")
run("U26.AD0", "U26.GND", "box", "Address 0x68")
run("U28.OE", "U28.GND", "box", "Outputs always enabled")
run("U28.SCL", "J10.6", "box", "Off-box I2C branch: steering column")
run("U28.SDA", "J10.7", "box")
run("A1:3V3", "J10.5", "box")
run("A1:GND", "J10.2", "box")
run("U28.SCL", "J11.3", "box", "Off-box I2C branch: dash")
run("U28.SDA", "J11.4", "box")
run("A1:3V3", "J11.1", "box")
run("A1:GND", "J11.2", "box")
run("U28.PWM0", "U29.IN1", "box", "Lights PWM")
run("U28.PWM1", "U29.IN2", "box", "Fan PWM")
run("U29.GND", "A1:GND", "box")

# ---- switch inputs
run("J4.1", "U24.P0", "box")
run("J4.2", "U24.P1", "box")
run("J4.3", "A1:GND", "box")
run("J4.1", "SW2.1", "stock")
run("SW2.2", "J4.3", "stock")
run("J4.2", "SW3.1", "stock")
run("SW3.2", "J4.3", "stock")
run("J8.3", "U24.P2", "box", "Horn button")
run("J8.4", "A1:GND", "box")
run("J8.3", "SW4.1", "stock")
run("SW4.2", "J8.4", "stock")
run("J8.1", "LP1.1", "stock", "Lights (voltage TBD)")
run("LP1.2", "J8.2", "stock")
run("J10.8", "U24.P4", "box", "Seat switch")
run("J10.3", "R23.2", "box", "WHEEL_SPEED_L to A1")
run("J10.4", "R24.2", "box", "WHEEL_SPEED_R to A1")

# ---- sensor harness (J10), our own
run("J10.1", "H1.1", "harness", "Split to both wheels")
run("J10.1", "H2.1", "harness")
run("J10.2", "H1.2", "harness")
run("J10.2", "H2.2", "harness")
run("J10.3", "H1.3", "harness")
run("J10.4", "H2.3", "harness")
run("J10.5", "U25.VCC", "harness", "AS5600: ≤ 30 cm of I2C, twisted with GND")
run("J10.2", "U25.GND", "harness")
run("J10.6", "U25.SCL", "harness")
run("J10.7", "U25.SDA", "harness")
run("U25.DIR", "U25.GND", "harness", "Jumper on the sensor board")
run("J10.8", "SW5.1", "harness")
run("SW5.2", "J10.2", "harness")

# ---- dash harness (J11) and music (J12)
run("J11.1", "U27.VCC", "harness")
run("J11.2", "U27.GND", "harness")
run("J11.3", "U27.SCL", "harness")
run("J11.4", "U27.SDA", "harness")
run("U30.6", "J11.5", "box")
run("U30.8", "J11.6", "box")
run("J11.5", "LS1.1", "harness")
run("J11.6", "LS1.2", "harness")
run("U32.L+", "J12.1", "box")
run("U32.L-", "J12.2", "box")
run("U32.R+", "J12.3", "box")
run("U32.R-", "J12.4", "box")
run("J12.1", "LS2.1", "harness")
run("J12.2", "LS2.2", "harness")
run("J12.3", "LS3.1", "harness")
run("J12.4", "LS3.2", "harness")

# ---- pins with no wire of their own, and why
DIRECT = {
    **{f"C11{i}.{p}": "470 µF leads under the driver's B+/B− screw terminals" for i in range(1, 6) for p in "12"},
    **{f"C20{i}.{p}": "Soldered across the motor tabs" for i in range(1, 6) for p in "12"},
    "D1.1": "Soldered across the K1 coil tabs", "D1.2": "Soldered across the K1 coil tabs",
    "D2.1": "TVS on TB2 / TB1 (not fitted yet)", "D2.2": "TVS on TB2 / TB1 (not fitted yet)",
    **{f"F{n}.1": "Fuse block common bus (fed by F3.1)" for n in (4, 5, 6, 7, 8)},
    **{f"R11{i}.1": "10k soldered inside the driver's control plug, EN to GND" for i in range(1, 6)},
    **{f"R11{i}.2": "10k soldered inside the driver's control plug, EN to GND" for i in range(1, 6)},
    "TB2.7": "Spare stud position (D2)", "TB2.8": "Ring lug holding the R5 and R7 leads",
    "R5.1": "Lead in a ring lug on TB2.8", "R7.1": "Lead in a ring lug on TB2.8",
    "U1.OUT-": "Joined to IN− inside the module", "U3.OUT-": "Joined to IN− inside the module",
    "U4.OUT-": "Joined to IN− inside the module", "U20.R14": "Joined to L14 on the board",
}
