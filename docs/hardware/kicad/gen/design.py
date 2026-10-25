"""The RideOnESP32 v3 electrical design as data: parts, pins and nets, sheets, connectors and wire runs.

Everything the KiCad files and the tables say comes from here. The ESP32 pin nets are read from
include/pins.h by generate.py, so this file never repeats a GPIO number.

Variant A (4 + 1 IBT-2) is the main design. Variant B (2 x MDD20A) is in VARIANT_B at the bottom.
"""

# ---------------------------------------------------------------- sources

ORDER = "Ordered 24 Oct"   # the AliExpress order of 24 Oct 2026 (shopping/2026-10-24-aliexpress-order.md)
OWNED = "Owned"
STOCK = "Stock car"
BUY = "To buy"
TBD = "TBD"

# ---------------------------------------------------------------- module symbols
# (number, name, electrical type); None leaves a gap. Numbers are the terminal or header names
# printed on the board where it has them.

I, O, B, P, PI, PO, OC, NC = "input", "output", "bidirectional", "passive", "power_in", "power_out", "open_collector", "no_connect"

MODULES = {
    "ESP32_DevKit_30pin": dict(ref="U", descr="ESP32-WROOM-32D 30-pin DevKit V1 (USB-C) on MRD068A screw-terminal adapter", left=[
        ("L1", "EN", I), ("L2", "GPIO36/VP", I), ("L3", "GPIO39/VN", I), ("L4", "GPIO34", I), ("L5", "GPIO35", I),
        ("L6", "GPIO32", B), ("L7", "GPIO33", B), ("L8", "GPIO25", B), ("L9", "GPIO26", B), ("L10", "GPIO27", B),
        ("L11", "GPIO14", B), ("L12", "GPIO12", B), ("L13", "GPIO13", B), ("L14", "GND", PI), ("L15", "VIN", PI)],
        right=[
        ("R1", "GPIO23", B), ("R2", "GPIO22", B), ("R3", "TX0/GPIO1", O), ("R4", "RX0/GPIO3", I), ("R5", "GPIO21", B),
        ("R6", "GPIO19", B), ("R7", "GPIO18", B), ("R8", "GPIO5", B), ("R9", "GPIO17", B), ("R10", "GPIO16", B),
        ("R11", "GPIO4", B), ("R12", "GPIO2", B), ("R13", "GPIO15", B), ("R14", "GND", PI), ("R15", "3V3", PO)]),
    "IBT2_BTS7960": dict(ref="U", descr="IBT-2 dual BTS7960 H-bridge board, 43 A", left=[
        ("1", "RPWM", I), ("2", "LPWM", I), ("3", "R_EN", I), ("4", "L_EN", I), ("5", "R_IS", P), ("6", "L_IS", P),
        ("7", "VCC", PI), ("8", "GND", PI)],
        right=[("B+", "B+", PI), ("B-", "B-", PI), None, ("M+", "M+", P), ("M-", "M-", P)]),
    "AION_RX_Mini": dict(ref="U", descr="Jumper AION RX Mini ExpressLRS 2.4 GHz receiver, CRSF",
                         left=[("5V", "5V", PI), ("GND", "GND", PI)], right=[("TX", "TX", O), ("RX", "RX", I)]),
    "DCDC_Buck_Module": dict(ref="U", descr="DC-DC buck converter module",
                             left=[("IN+", "IN+", PI), ("IN-", "IN-", PI)], right=[("OUT+", "OUT+", PO), ("OUT-", "OUT-", PI)]),
    "AMS1117_Module": dict(ref="U", descr="AMS1117-3.3 regulator module",
                           left=[("VIN", "VIN", PI), ("GND", "GND", PI)], right=[("VOUT", "VOUT", PO)]),
    "ADS1115_Module": dict(ref="U", descr="ADS1115 16-bit 4-channel ADC module (I2C)", left=[
        ("VDD", "VDD", PI), ("GND", "GND", PI), ("SCL", "SCL", I), ("SDA", "SDA", B), ("ADDR", "ADDR", I), ("ALRT", "ALRT", OC)],
        right=[("A0", "A0", I), ("A1", "A1", I), ("A2", "A2", I), ("A3", "A3", I)]),
    "PCF8574_Module": dict(ref="U", descr="PCF8574 8-bit I/O expander module (I2C), address set by A0-A2 jumpers", left=[
        ("VCC", "VCC", PI), ("GND", "GND", PI), ("SDA", "SDA", B), ("SCL", "SCL", I), ("INT", "INT", OC)],
        right=[(f"P{i}", f"P{i}", B) for i in range(8)]),
    "AS5600_Module": dict(ref="U", descr="AS5600 magnetic angle sensor module with diametric magnet", left=[
        ("VCC", "VCC", PI), ("GND", "GND", PI), ("SCL", "SCL", I), ("SDA", "SDA", B)],
        right=[("DIR", "DIR", I), ("OUT", "OUT", O), ("GPO", "GPO", O)]),
    "GY521_MPU6050": dict(ref="U", descr="GY-521 MPU-6050 6-axis IMU module", left=[
        ("VCC", "VCC", PI), ("GND", "GND", PI), ("SCL", "SCL", I), ("SDA", "SDA", B)],
        right=[("XDA", "XDA", B), ("XCL", "XCL", O), ("AD0", "AD0", I), ("INT", "INT", O)]),
    "SSD1306_091_OLED": dict(ref="U", descr="0.91 in 128x32 SSD1306 OLED module (I2C)",
                             left=[("GND", "GND", PI), ("VCC", "VCC", PI)], right=[("SCL", "SCL", I), ("SDA", "SDA", B)]),
    "PCA9685_Board": dict(ref="U", descr="PCA9685 16-channel 12-bit PWM board (I2C)", left=[
        ("GND", "GND", PI), ("OE", "OE", I), ("SCL", "SCL", I), ("SDA", "SDA", B), ("VCC", "VCC", PI), ("V+", "V+", P)],
        right=[(f"PWM{i}", f"PWM{i}", O) for i in range(16)]),
    "MOSFET_Board_4ch": dict(ref="U", descr="4-channel low-side MOSFET switch board, logic-level inputs", left=[
        ("IN1", "IN1", I), ("IN2", "IN2", I), ("IN3", "IN3", I), ("IN4", "IN4", I), ("GND", "GND", PI)],
        right=[("OUT1", "OUT1", P), ("OUT2", "OUT2", P), ("OUT3", "OUT3", P), ("OUT4", "OUT4", P), ("PGND", "PGND", P)]),
    "DFPlayer_Mini": dict(ref="U", descr="DFPlayer Mini MP3 module with 3 W speaker amp (pin numbers are the module's)", left=[
        ("1", "VCC", PI), ("7", "GND", PI), ("2", "RX", I), ("3", "TX", O)],
        right=[("6", "SPK_1", P), ("8", "SPK_2", P), ("16", "BUSY", O)]),
    "BT_Amp_ZK502T": dict(ref="U", descr="ZK-502T 2 x 50 W Bluetooth amplifier board (Bluetooth on this board only)",
                          left=[("VIN+", "VIN+", PI), ("VIN-", "VIN-", PI)],
                          right=[("L+", "L+", P), ("L-", "L-", P), ("R+", "R+", P), ("R-", "R-", P)]),
    "DS18B20_Probe": dict(ref="U", descr="DS18B20 waterproof temperature probe (red VDD, yellow DQ, black GND)",
                          left=[("VDD", "VDD", PI), ("GND", "GND", PI)], right=[("DQ", "DQ", B)]),
    "A3144_Hall": dict(ref="H", descr="A3144 unipolar hall switch, open-collector output",
                       left=[("1", "VCC", PI), ("2", "GND", PI)], right=[("3", "OUT", OC)]),
    "Contactor_SW80": dict(ref="K", descr="Main contactor, 24 V coil, single-pole NO, studs + coil tabs",
                           left=[("C+", "COIL+", P), ("C-", "COIL-", P)], right=[("A", "STUD A", P), ("B", "STUD B", P)]),
    "EStop_2NO_2NC": dict(ref="S", descr="22 mm latching mushroom E-stop, 2 NC + 2 NO contact blocks (IEC numbering)", left=[
        ("11", "NC1", P), ("12", "NC1", P), ("21", "NC2", P), ("22", "NC2", P)],
        right=[("33", "NO3", P), ("34", "NO3", P), ("43", "NO4", P), ("44", "NO4", P)]),
    "Pedal_TBD": dict(ref="SW", descr="Stock foot pedal: switch, potentiometer or hall throttle (TBD, issue #16)",
                      left=[("V+", "V+", P), ("GND", "GND", P)], right=[("SIG", "SIG", P)]),
    "Stud_Bus": dict(ref="TB", descr="Bolted stud or bus bar with ring lugs", left=[(str(i), "", P) for i in range(1, 9)], right=[]),
    "Stud_Bus12": dict(ref="TB", descr="Bolted stud or bus bar with ring lugs",
                       left=[(str(i), "", P) for i in range(1, 7)], right=[(str(i), "", P) for i in range(7, 13)]),
    "Current_Sensor_TBD": dict(ref="U", descr="Hall current sensor in one motor lead (part TBD, issue #32)",
                               left=[("IP+", "IP+", P), ("IP-", "IP-", P)], right=[("VCC", "VCC", PI), ("GND", "GND", PI), ("OUT", "OUT", O)]),
    "MDD20A": dict(ref="U", descr="Cytron MDD20A dual 20 A motor driver (PWM + DIR per channel, no enable, no current sense)", left=[
        ("GND", "GND", PI), ("PWM1", "PWM1", I), ("DIR1", "DIR1", I), ("PWM2", "PWM2", I), ("DIR2", "DIR2", I)],
        right=[("VB+", "VB+", PI), ("VB-", "VB-", PI), ("M1A", "M1A", P), ("M1B", "M1B", P), ("M2A", "M2A", P), ("M2B", "M2B", P)]),
    "74LVC08": dict(ref="U", descr="74LVC08 quad 2-input AND (SOIC/DIP on the interface board)", left=[
        ("1", "1A", I), ("2", "1B", I), ("4", "2A", I), ("5", "2B", I), ("9", "3A", I), ("10", "3B", I),
        ("12", "4A", I), ("13", "4B", I), ("14", "VCC", PI), ("7", "GND", PI)],
        right=[("3", "1Y", O), ("6", "2Y", O), ("8", "3Y", O), ("11", "4Y", O)]),
}

# ---------------------------------------------------------------- parts


class Part:
    def __init__(s, ref, sym, value, pins, part="", source="", loc="box", note="", dnp=False, in_bom=True,
                 rot=None, datasheet="", issue=""):
        s.ref, s.sym, s.value, s.pins = ref, sym, value, dict(pins)
        s.part, s.source, s.loc, s.note, s.dnp, s.in_bom = part, source, loc, note, dnp, in_bom
        s.rot, s.datasheet, s.issue = rot, datasheet, issue
        s.descr = part
        s.sheet = None

    @property
    def fields(s):
        f = {"Part": s.part, "Source": s.source, "Location": s.loc}
        if s.note:
            f["Note"] = s.note
        if s.issue:
            f["Issue"] = s.issue
        return f


def M(name):
    return "rideonesp32:" + name


def R(ref, value, a, b, part=None, loc="A1", source=ORDER, note="", dnp=False):
    return Part(ref, "Device:R", value, {"1": a, "2": b}, part or f"Resistor {value}, metal film 1 %, 0.25 W",
                source, loc, note, dnp)


def C(ref, value, a, b, part=None, loc="A1", source=ORDER, note="", dnp=False):
    return Part(ref, "Device:C", value, {"1": a, "2": b}, part or f"Ceramic capacitor {value}, 50 V", source, loc, note, dnp)


def CP(ref, value, pos, neg, part, loc, source=ORDER, note=""):
    return Part(ref, "Device:C_Polarized", value, {"1": pos, "2": neg}, part, source, loc, note)


def D(ref, value, k, a, part, loc="A1", source=ORDER, note="", dnp=False):
    return Part(ref, "Device:D", value, {"1": k, "2": a}, part, source, loc, note, dnp)


def F(ref, value, a, b, part, loc="box", source=ORDER, note=""):
    return Part(ref, "Device:Fuse", value, {"1": a, "2": b}, part, source, loc, note)


def J(ref, n, value, nets, part, source=TBD, loc="box edge", note="", issue=""):
    return Part(ref, f"Connector_Generic:Conn_01x{n:02d}", value, {str(i + 1): net for i, net in enumerate(nets)},
                part, source, loc, note, issue=issue)


def PWR(ref, net):
    return Part(ref, "power:PWR_FLAG", "PWR_FLAG", {"1": net}, "", "", "", in_bom=False)


CH = ["FL", "FR", "RL", "RR"]
CH_NAME = {"FL": "front-left", "FR": "front-right", "RL": "rear-left", "RR": "rear-right", "STEER": "steering"}
DRV_REF = {"FL": "U10", "FR": "U11", "RL": "U12", "RR": "U13", "STEER": "U14"}
MOTOR_REF = {"FL": "M1", "FR": "M2", "RL": "M3", "RR": "M4", "STEER": "M5"}

# ---- Sheet 3: car side and the box edge -------------------------------------------------------

car = [
    Part("BT1", "Device:Battery", "24 V pack", {"1": "BATT_RAW", "2": "GND"}, "Stock 24 V battery; voltage, Ah, chemistry TBD",
         STOCK, "car", "BTS7960 locks out above 27.5 V; check the charger voltage", issue="#13"),
    F("F1", "40 A ANL", "BATT_RAW", "BATT_FUSED", "ANL fuse holder + 40 A ANL fuse", "car, ≤15 cm from battery +",
      note="Unfused wire before F1 must stay under 15 cm. If the stock harness already has a 40 A fuse this close, that fuse is F1"),
    J("J1", 2, "BATTERY", ["BATT_FUSED", "GND"], "Mate of the stock battery plug (fallback: Anderson SB50)", issue="#17"),
    Part("SW1", "Switch:SW_SPST", "Key switch", {"1": "KEY_FEED", "2": "KEY_24V"}, "Stock dash key switch (rating TBD; if < 10 A, drive a 30 A relay from it)",
         STOCK, "dash", issue="#17"),
    J("J2", 2, "KEY", ["KEY_FEED", "KEY_24V"], "Mate of the stock key-switch plug", issue="#17"),
    Part("PED1", M("Pedal_TBD"), "Foot pedal", {"V+": "PEDAL_VCC", "GND": "GND", "SIG": "PEDAL_IN"},
         "Stock pedal: switch, pot or hall throttle (TBD). Never a line that carries 24 V", STOCK, "car", issue="#16"),
    J("J3", 3, "PEDAL", ["PEDAL_VCC", "PEDAL_IN", "GND"], "Mate of the stock pedal plug", issue="#17"),
    Part("SW2", "Switch:SW_SPST", "Shift F", {"1": "SHIFT_FORWARD", "2": "GND"}, "Stock shifter, forward contact (must be signal-only)", STOCK, "car", issue="#17"),
    Part("SW3", "Switch:SW_SPST", "Shift R", {"1": "SHIFT_REVERSE", "2": "GND"}, "Stock shifter, reverse contact (must be signal-only)", STOCK, "car", issue="#17"),
    J("J4", 3, "SHIFTER", ["SHIFT_FORWARD", "SHIFT_REVERSE", "GND"], "Mate of the stock shifter plug", issue="#17"),
    J("J5", 2, "STEER MOTOR", ["STEER_M_POS", "STEER_M_NEG"], "Mate of the stock steering-motor plug (fallback: XT60, owned)", issue="#17"),
    Part("M5", "Motor:Motor_DC", "Steering motor", {"1": "STEER_M_POS", "2": "STEER_M_NEG"}, "Stock steering motor (label TBD)", STOCK, "car", issue="#14"),
    J("J6", 4, "FRONT MOTORS", ["FL_M_POS", "FL_M_NEG", "FR_M_POS", "FR_M_NEG"],
      "Mate of the stock front-motor plug(s); if the harness pairs the motors, a reversible splitter at the motor plugs (fallback: 2 x XT60)",
      issue="#17, #33"),
    J("J7", 4, "REAR MOTORS", ["RL_M_POS", "RL_M_NEG", "RR_M_POS", "RR_M_NEG"],
      "Mate of the stock rear-motor plug(s); splitter as J6 (fallback: 2 x XT60)", issue="#17, #33"),
] + [
    Part(MOTOR_REF[c], "Motor:Motor_DC", f"{c} motor", {"1": f"{c}_M_POS", "2": f"{c}_M_NEG"},
         f"Stock {CH_NAME[c]} drive motor (label TBD)", STOCK, "car", issue="#14") for c in CH
] + [
    C(f"C20{i + 1}", "100n", f"{c}_M_POS", f"{c}_M_NEG", "Ceramic capacitor 100 nF 50 V, across the motor terminals",
      loc="car, at motor", note="Noise suppression; solder right on the motor tabs") for i, c in enumerate(CH)
] + [
    C("C205", "100n", "STEER_M_POS", "STEER_M_NEG", "Ceramic capacitor 100 nF 50 V, across the motor terminals", loc="car, at motor"),
    J("J8", 4, "ACCESSORIES", ["LIGHTS_12V", "LIGHTS_SW", "HORN", "GND"],
      "Mate of the stock lights/horn plug (pin use TBD from photos)", issue="#17"),
    Part("SW4", "Switch:SW_Push", "Horn", {"1": "HORN", "2": "GND"}, "Stock horn button", STOCK, "dash", issue="#17"),
    Part("LP1", "Device:R", "Lights", {"1": "LIGHTS_12V", "2": "LIGHTS_SW"}, "Stock lights (load shown as a resistor; voltage TBD)",
         STOCK, "car", in_bom=False, issue="#17"),
    Part("S2", M("EStop_2NO_2NC"), "E-STOP", {"11": "COIL_FEED", "12": "ESTOP_MID", "21": "ESTOP_MID", "22": "COIL_24V",
                                               "33": "ESTOP_SENSE", "34": "GND", "43": None, "44": None},
         "22 mm latching E-stop 2NO + 2NC", ORDER, "dash",
         "Both NC blocks in series with the K1 coil (a welded NC block still leaves one that opens). NO3 reads the state"),
    J("J9", 4, "E-STOP", ["COIL_FEED", "COIL_24V", "ESTOP_SENSE", "GND"],
      "Sealed 4-way, Deutsch DT04-4P / DT06-4S (or DT-style clone)", BUY, note="Our own plug: E-stop on the dash"),
    Part("H1", M("A3144_Hall"), "A3144", {"1": "5V_LOGIC", "2": "GND", "3": "WHEEL_SPEED_L"}, "A3144 hall switch + magnet on the left wheel", BUY, "car, left wheel"),
    Part("H2", M("A3144_Hall"), "A3144", {"1": "5V_LOGIC", "2": "GND", "3": "WHEEL_SPEED_R"}, "A3144 hall switch + magnet on the right wheel", BUY, "car, right wheel"),
    Part("U25", M("AS5600_Module"), "AS5600 0x36", {"VCC": "3V3_SENSE", "GND": "GND", "SCL": "I2C_SCL", "SDA": "I2C_SDA",
                                                     "DIR": "GND", "OUT": None, "GPO": None},
         "AS5600 module + diametric magnet", ORDER, "car, steering column",
         "Over ~30 cm of cable: use OUT on ADS#2 A1, or a bus buffer (TBD #11 in v0.2)"),
    Part("SW5", "Switch:SW_SPST", "Seat", {"1": "SEAT", "2": "GND"}, "Seat switch, closes with a child seated (optional)", BUY, "car, seat"),
    J("J10", 8, "SENSORS", ["5V_LOGIC", "GND", "WHEEL_SPEED_L", "WHEEL_SPEED_R", "3V3_SENSE", "I2C_SCL", "I2C_SDA", "SEAT"],
      "Sealed 8-way, Deutsch DT04-08PA / DT06-08SA (or clone)", BUY, note="Our own plug: halls, steering angle, seat"),
    Part("U27", M("SSD1306_091_OLED"), "OLED 0x3C", {"GND": "GND", "VCC": "3V3_SENSE", "SCL": "I2C_SCL", "SDA": "I2C_SDA"},
         "0.91 in SSD1306 OLED", OWNED, "dash", "On 3.3 V so its pull-ups can't lift the bus to 5 V"),
    Part("LS1", "Device:Speaker", "Alert spk", {"1": "SPK_ALERT_P", "2": "SPK_ALERT_N"}, "3 in 4 Ω full-range speaker, car sounds", BUY, "dash"),
    J("J11", 6, "DASH", ["3V3_SENSE", "GND", "I2C_SCL", "I2C_SDA", "SPK_ALERT_P", "SPK_ALERT_N"],
      "Sealed 6-way, Deutsch DT04-6P / DT06-6S (or clone)", BUY, note="Our own plug: OLED and alert speaker"),
    Part("LS2", "Device:Speaker", "Music L", {"1": "SPK_L_P", "2": "SPK_L_N"}, "3 in full-range speaker (+ DDT-S30 tweeter via 10 µF film cap, AUDIO.md)", BUY, "car"),
    Part("LS3", "Device:Speaker", "Music R", {"1": "SPK_R_P", "2": "SPK_R_N"}, "3 in full-range speaker (+ DDT-S30 tweeter via 10 µF film cap, AUDIO.md)", BUY, "car"),
    J("J12", 4, "MUSIC", ["SPK_L_P", "SPK_L_N", "SPK_R_P", "SPK_R_N"], "4-way, Deutsch DT04-4P / DT06-4S (or clone)", BUY,
      note="Our own plug; music path only, never shared with the ESP32"),
]

# ---- Sheet 4: power and safety chain ------------------------------------------------------------

power = [
    Part("K1", M("Contactor_SW80"), "125 A 24 V", {"C+": "COIL_24V", "C-": "COIL_LOW", "A": "BATT_FUSED", "B": "MOTOR_24V"},
         "ZJW100 (SW80 type) contactor, 125 A, 24 V coil", ORDER, "box (may mount beside it: ~10 cm tall)",
         "Measure its coil current on the bench; if over 0.6 A make F3 2 A"),
    D("D1", "1N4007", "COIL_24V", "COIL_LOW", "1N4007, flyback across the K1 coil", loc="box, on K1 coil tabs",
      note="A plain diode slows K1 opening a little; a 1N4007 + 24 V zener in series opens faster (optional)"),
    Part("Q1", "Device:Q_NMOS", "IRLZ44N", {"G": "Q1_GATE", "D": "COIL_LOW", "S": "GND"}, "IRLZ44N logic-level N-MOSFET, TO-220", ORDER, "A1",
         "The ESP32 can open K1 but never close it against the key or E-stop"),
    R("R1", "100", "CONTACTOR_HOLD", "Q1_GATE", note="Gate resistor"),
    R("R2", "10k", "CONTACTOR_HOLD", "GND", note="Holds K1 open and strapping pin GPIO12 LOW at boot. Never a pull-up"),
    F("F2", "15 A", "BATT_FUSED", "KEY_FEED", "Inline blade fuse holder (12 AWG) + 15 A blade", loc="box, on K1 stud A",
      note="Feeds the key; protects the key wiring"),
    F("F3", "1 A", "KEY_24V", "COIL_FEED", "Blade fuse 1 A in the fuse block", note="K1 coil chain"),
    F("F4", "2 A", "KEY_24V", "U1_IN", "Blade fuse 2 A in the fuse block", note="5V_LOGIC buck input"),
    F("F5", "3 A", "KEY_24V", "U3_IN", "Blade fuse 3 A in the fuse block", note="5V_ACC buck input"),
    F("F6", "10 A", "KEY_24V", "U4_IN", "Blade fuse 10 A in the fuse block", note="12V_ACC buck input"),
    F("F7", "1 A", "KEY_24V", "FAN_24V", "Blade fuse 1 A in the fuse block", note="Box fans"),
    F("F8", "1 A", "KEY_24V", "BATT_DIV", "Blade fuse 1 A in the fuse block", note="Battery divider wire (0.5 A in v0.2; 1 A is in the order)"),
    R("R3", "150k", "BATT_DIV", "BATTERY_SENSE", note="2.2 V at 29.4 V"),
    R("R4", "12k", "BATTERY_SENSE", "GND"),
    C("C1", "100n", "BATTERY_SENSE", "GND"),
    R("R5", "150k", "MOTOR_24V", "BUS_SENSE", loc="box, on TB2.8",
      note="Motor-bus sense for the K1 checks; at the stud, so only the divided signal runs to A1"),
    R("R6", "12k", "BUS_SENSE", "GND", note="2.2 V at 29.4 V, as the battery divider"),
    C("C2", "100n", "BUS_SENSE", "GND"),
    R("R7", "2.2k 3W", "MOTOR_24V", "GND", part="Resistor 2.2k, 3 W wirewound or metal oxide", loc="box, on TB2.8",
      source=BUY, note="Bleed: drains the driver caps with K1 open (5 s time constant), so the boot weld check can see a welded K1. 0.4 W with K1 closed"),
    Part("TB1", M("Stud_Bus12"), "GND star", {str(i): "GND" for i in range(1, 13)}, "M6 brass stud or bus bar: the one ground star point",
         BUY, "box", "Battery −, every driver B−, rails and logic ground meet here and nowhere else"),
    Part("TB2", M("Stud_Bus"), "MOTOR_24V bus", {str(i): "MOTOR_24V" for i in range(1, 9)}, "M6 stud or bus bar after K1 stud B", BUY, "box"),
    Part("D2", "Device:D_TVS", "TVS TBD", {"1": "MOTOR_24V", "2": "GND"}, "Unidirectional TVS across MOTOR_24V, voltage set by the battery (TBD)",
         TBD, "box, at TB2", "Clamps the regen spike when K1 opens on a rolling car. Fit once the battery voltage is known", dnp=True, issue="#13"),
    PWR("#FLG01", "MOTOR_24V"), PWR("#FLG02", "GND"), PWR("#FLG03", "U1_IN"), PWR("#FLG04", "U3_IN"), PWR("#FLG05", "U4_IN"),
]

# ---- Sheet 5: rails ------------------------------------------------------------------------------

rails = [
    Part("U1", M("DCDC_Buck_Module"), "LM2596 → 5.0 V", {"IN+": "U1_IN", "IN-": "GND", "OUT+": "5V_LOGIC", "OUT-": "GND"},
         "LM2596 buck module set to 5.0 V (set it before connecting the ESP32)", OWNED, "box", "ESP32 VIN, receiver, halls, U2. About 0.5 A"),
    Part("U2", M("AMS1117_Module"), "AMS1117-3.3", {"VIN": "5V_LOGIC", "GND": "GND", "VOUT": "3V3_SENSE"},
         "AMS1117-3.3 module", ORDER, "box", "3V3_SENSE: IBT-2 logic, I2C devices, clamps, pedal. About 0.1 A"),
    Part("U3", M("DCDC_Buck_Module"), "24 → 5 V 5 A", {"IN+": "U3_IN", "IN-": "GND", "OUT+": "5V_ACC", "OUT-": "GND"},
         "24 to 5 V 5 A buck module", BUY, "box", "Accessories only: DFPlayer, USB, LED strips"),
    Part("U4", M("DCDC_Buck_Module"), "24 → 12 V 10 A", {"IN+": "U4_IN", "IN-": "GND", "OUT+": "12V_ACC", "OUT-": "GND"},
         "24 to 12 V 10-20 A sealed buck", BUY, "box", "Lights and music amp"),
]

# ---- Sheet 6: drive, variant A -------------------------------------------------------------------


def drive_channel(c, n):
    en = "STEER_EN" if c == "STEER" else "DRIVE_EN"
    pre = "STEER" if c == "STEER" else c
    isn = "IS_ST" if c == "STEER" else f"IS_{c}"
    return [
        Part(DRV_REF[c], M("IBT2_BTS7960"), f"IBT-2 {c}", {
            "1": f"{pre}_RPWM", "2": f"{pre}_LPWM", "3": en, "4": en, "5": f"{isn}_RAW", "6": f"{isn}_RAW",
            "7": "3V3_SENSE", "8": "GND", "B+": "MOTOR_24V", "B-": "GND", "M+": f"{pre}_M_POS", "M-": f"{pre}_M_NEG"},
            f"IBT-2 (BTS7960) board, {CH_NAME[c]}", OWNED if c != "STEER" else BUY, "box",
            "VCC on 3.3 V so 3.3 V PWM reads as a clean HIGH. Never tie R_EN/L_EN to VCC"),
        R(f"R10{n}", "4.7k", f"{isn}_RAW", isn, note="IS series resistor"),
        C(f"C10{n}", "10n", isn, "GND", note="IS filter"),
        D(f"D10{n}", "1N4148", "3V3_SENSE", isn, "1N4148 (ordered high-speed switching diodes), IS clamp to 3.3 V",
          note="Clamps the 4-7 mA fault current so the ADS1115 input stays ≤ 3.6 V"),
        CP(f"C11{n}", "470µ 50V", "MOTOR_24V", "GND", "Electrolytic 470 µF ≥35 V low-ESR (50 V preferred)", f"box, on {DRV_REF[c]} B+/B−",
           note="CHECK the ordered caps are ≥35 V: the order line reads '10-25 V'"),
        R(f"R11{n}", "10k", en, "GND", loc=f"box, at {DRV_REF[c]} header",
          note="Pull-down at each board, so a broken enable wire still leaves that board off"),
    ]


drive = []
for i, c in enumerate(CH + ["STEER"]):
    drive += drive_channel(c, i + 1)

# ---- Sheet 7: controller -------------------------------------------------------------------------

controller = [
    Part("U20", M("ESP32_DevKit_30pin"), "ESP32-WROOM-32D", {}, "ESP32-WROOM-32D 30-pin DevKit, USB-C, on MRD068A", OWNED, "box",
         "Pins from include/pins.h. 3V3 pin unused: sensors run from U2"),
    Part("U21", M("AION_RX_Mini"), "AION RX Mini", {"5V": "5V_LOGIC", "GND": "GND", "TX": "CRSF_RX", "RX": "CRSF_TX"},
         "Jumper AION RX Mini ELRS receiver", OWNED, "box (antenna out of the box, away from motor wires)", "CRSF 420000 baud"),
    R("R22", "100", "3V3_SENSE", "PEDAL_VCC", note="Pedal supply; a shorted pedal cable can't pull down 3V3_SENSE"),
    R("R20", "10k", "PEDAL_IN", "PEDAL", note="Series protection"),
    R("R21", "100k", "PEDAL_IN", "GND", note="Unplugged pedal reads 0 (released)"),
    C("C20", "100n", "PEDAL", "GND"),
    D("D20", "1N4148", "3V3_SENSE", "PEDAL", "1N4148, clamp to 3.3 V"),
    R("R23", "10k", "3V3_SENSE", "WHEEL_SPEED_L", note="GPIO34 has no internal pull-up"),
    C("C21", "100n", "WHEEL_SPEED_L", "GND"),
    R("R24", "10k", "3V3_SENSE", "WHEEL_SPEED_R", note="GPIO35 has no internal pull-up"),
    C("C22", "100n", "WHEEL_SPEED_R", "GND"),
    R("R27", "4.7k", "3V3_SENSE", "ONEWIRE_TEMP", note="1-Wire pull-up; also keeps strapping pin GPIO15 HIGH"),
    Part("U31", M("DS18B20_Probe"), "DS18B20", {"VDD": "3V3_SENSE", "GND": "GND", "DQ": "ONEWIRE_TEMP"},
         "DS18B20 waterproof probe", BUY, "box, on the hottest driver heatsink"),
    R("R28", "1k", "SOUND_TX", "DF_RX", note="DFPlayer RX series resistor"),
]

# ---- Sheet 8: I2C bus and inputs -----------------------------------------------------------------

i2c = [
    R("R25", "4.7k", "3V3_SENSE", "I2C_SCL", dnp=True,
      note="Fit only if the module pull-ups in parallel measure above 4.7k. Seven modules with their own 10k/4.7k already give ~1-2k"),
    R("R26", "4.7k", "3V3_SENSE", "I2C_SDA", dnp=True, note="As R25"),
    Part("U22", M("ADS1115_Module"), "ADS1115 0x48", {"VDD": "3V3_SENSE", "GND": "GND", "SCL": "I2C_SCL", "SDA": "I2C_SDA", "ADDR": "GND",
                                                     "ALRT": None, "A0": "IS_FL", "A1": "IS_FR", "A2": "IS_RL", "A3": "IS_RR"},
         "ADS1115 module", ORDER, "box", "Drive current sense"),
    Part("U23", M("ADS1115_Module"), "ADS1115 0x49", {"VDD": "3V3_SENSE", "GND": "GND", "SCL": "I2C_SCL", "SDA": "I2C_SDA", "ADDR": "3V3_SENSE",
                                                     "ALRT": None, "A0": "IS_ST", "A1": None, "A2": "BUS_SENSE", "A3": None},
         "ADS1115 module", ORDER, "box", "Steering current sense (A0), motor bus (A2); A1 reserved for AS5600 OUT, A3 spare"),
    Part("U24", M("PCF8574_Module"), "PCF8574 0x20", {"VCC": "3V3_SENSE", "GND": "GND", "SDA": "I2C_SDA", "SCL": "I2C_SCL", "INT": None,
                                                     "P0": "SHIFT_FORWARD", "P1": "SHIFT_REVERSE", "P2": "HORN", "P3": "ESTOP_SENSE",
                                                     "P4": "SEAT", "P5": None, "P6": None, "P7": None},
         "PCF8574 module, A2-A0 jumpers all to GND", ORDER, "box", "Switches pull to GND: 0 = pressed"),
    Part("U26", M("GY521_MPU6050"), "MPU-6050 0x68", {"VCC": "3V3_SENSE", "GND": "GND", "SCL": "I2C_SCL", "SDA": "I2C_SDA",
                                                     "XDA": None, "XCL": None, "AD0": "GND", "INT": None},
         "GY-521 MPU-6050", OWNED, "box, bolted flat to the floor plate", "Tilt and crash (later)"),
    Part("U28", M("PCA9685_Board"), "PCA9685 0x40", {"GND": "GND", "OE": "GND", "SCL": "I2C_SCL", "SDA": "I2C_SDA", "VCC": "3V3_SENSE", "V+": None,
                                                    **{f"PWM{i}": None for i in range(16)}, "PWM0": "LIGHTS_PWM", "PWM1": "FAN_PWM"},
         "PCA9685 16-channel PWM board", BUY, "box", "Accessory PWM; PWM2-15 spare"),
]

# ---- Sheet 9: accessories ------------------------------------------------------------------------

acc = [
    Part("U29", M("MOSFET_Board_4ch"), "4ch MOSFET", {"IN1": "LIGHTS_PWM", "IN2": "FAN_PWM", "IN3": None, "IN4": None, "GND": "GND",
                                                     "OUT1": "LIGHTS_SW", "OUT2": "FAN_LOW", "OUT3": None, "OUT4": None, "PGND": "GND"},
         "4-channel low-side MOSFET board, logic-level", BUY, "box"),
    Part("FAN1", "Motor:Fan", "24 V 80 mm", {"1": "FAN_24V", "2": "FAN_LOW"}, "24 V 80 mm fan, finger guard, filter (intake)", BUY, "box wall"),
    Part("FAN2", "Motor:Fan", "24 V 80 mm", {"1": "FAN_24V", "2": "FAN_LOW"}, "24 V 80 mm fan, finger guard (exhaust)", BUY, "box wall"),
    F("F9", "3 A", "12V_ACC", "LIGHTS_12V", "Inline blade fuse holder (ordered, 3 pcs) + 3 A", note="Lights branch of the 10-20 A buck"),
    F("F10", "10 A", "12V_ACC", "AMP_12V", "Inline blade fuse holder (ordered, 3 pcs) + 10 A", note="Music amp branch"),
    Part("U32", M("BT_Amp_ZK502T"), "ZK-502T", {"VIN+": "AMP_12V", "VIN-": "GND", "L+": "SPK_L_P", "L-": "SPK_L_N", "R+": "SPK_R_P", "R-": "SPK_R_N"},
         "ZK-502T Bluetooth amp", BUY, "box or dash", "Cap the volume (EN 71-1 loudness, see the safety review)"),
    Part("U30", M("DFPlayer_Mini"), "DFPlayer Mini", {"1": "5V_ACC", "7": "GND", "2": "DF_RX", "3": None, "6": "SPK_ALERT_P", "8": "SPK_ALERT_N", "16": None},
         "DFPlayer Mini + micro-SD card", BUY, "box"),
    PWR("#FLG07", "AMP_12V"),
]

# ---- Sheets ---------------------------------------------------------------------------------------
# Each sheet: file, title, columns of (title, [refs or Note keys]).

SHEETS = [
    dict(file="harness", blurb='Everything outside the box: battery + F1, key, pedal,\nshifter, 5 motors, E-stop, sensors, dash, speakers.\nBox plugs J1-J12 (J1-J8 mate the stock plugs, TBD #17).', name="Car harness and box edge", parts=car, comments=[
        "Left: what stays in the car (stock parts and our off-box parts). Right of each plug: into the box.",
        "Plugs facing the stock harness (J1-J8) are TBD until the stock plug photos (#17).",
        "J9-J12 are our own sealed plugs."], columns=[
        ("Battery and key", ["BT1", "F1", "J1", "SW1", "J2"]),
        ("Pedal and shifter", ["PED1", "J3", "SW2", "SW3", "J4"]),
        ("Front motors", ["M1", "C201", "M2", "C202", "J6"]),
        ("Rear motors", ["M3", "C203", "M4", "C204", "J7"]),
        ("Steering motor, E-stop", ["M5", "C205", "J5", "S2", "J9"]),
        ("---", []),
        ("Horn and lights", ["SW4", "LP1", "J8", "NOTE:going_back"]),
        ("Sensors", ["H1", "H2", "U25", "SW5", "J10"]),
        ("Dash", ["U27", "LS1", "J11"]),
        ("Music", ["LS2", "LS3", "J12"]),
    ]),
    dict(file="power", blurb='K1 contactor and its coil chain: key, E-stop (2 NC),\nQ1 hold MOSFET. F2 key feed, fuse block F3-F8,\nground star TB1, motor bus TB2, battery divider,\nmotor-bus sense and bleed.', name="Power and safety chain", parts=power, comments=[
        "Layer 1, no software: key and E-stop in series with the K1 coil; either one opens K1 whatever the ESP32 does.",
        "Q1 lets the ESP32 open K1 but never close it. Reset, crash or brown-out opens it (R2 pull-down).",
        "Rails come from the key side, before K1, so the ESP32 stays up after an E-stop and can report why.",
        "R5-R7: the ESP32 reads MOTOR_24V on U23 A2. At boot K1 stays open until R7 has drained the bus; still live = K1 welded."], columns=[
        ("Main path", ["K1", "TB2", "D2", "TB1"]),
        ("Coil chain", ["F2", "F3", "D1", "Q1", "R1", "R2", "NOTE:coil_chain"]),
        ("Fuse block (key side)", ["F4", "F5", "F6", "F7", "F8", "R3", "R4", "C1"]),
        ("Motor bus sense", ["R5", "R6", "C2", "R7"]),
        ("Notes", ["NOTE:main_path", "NOTE:fuses", "#FLG01", "#FLG02", "#FLG03", "#FLG04", "#FLG05"]),
    ]),
    dict(file="rails", blurb='U1 LM2596 5V_LOGIC, U2 AMS1117 3V3_SENSE,\nU3 5V_ACC and U4 12V_ACC for accessories.', name="Low-voltage rails", parts=rails, comments=[
        "Logic and accessories never share a converter. 3V3_SENSE comes from 5V_LOGIC, not the ESP32's own 3.3 V pin.",
        "Set each buck's output with a meter before connecting any load."], columns=[
        ("Logic", ["U1", "U2"]),
        ("Accessories", ["U3", "U4"]),
        ("Budget", ["NOTE:rails"]),
    ]),
    dict(file="drive", blurb='U10-U13 IBT-2 per wheel, U14 steering IBT-2.\n470 µF each, enable pull-down per board,\nIS current-sense networks to the ADS1115s.', name="Drive, variant A (4 + 1 IBT-2)", parts=drive, comments=[
        "Layer 2: DRIVE_EN and STEER_EN are pulled down at every board, so boot, reset, flashing and crashes leave the drivers off.",
        "IS sense: R_IS + L_IS joined (only one side conducts at a time) -> 4.7k -> ADS1115, 10 nF filter, clamp to 3.3 V.",
        "Variant B (2 x MDD20A) replaces this sheet: see variant_b/."], columns=[
        (f"{c} ({CH_NAME[c]})", [DRV_REF[c], f"C11{i + 1}", f"R11{i + 1}", f"R10{i + 1}", f"C10{i + 1}", f"D10{i + 1}"])
        for i, c in enumerate(CH + ["STEER"])
    ]),
    dict(file="controller", blurb='ESP32 (nets from include/pins.h), ELRS receiver,\npedal input protection, wheel-speed pull-ups,\nDS18B20, DFPlayer serial.', name="Controller and inputs", parts=controller, comments=[
        "Pin nets come from include/pins.h; CI keeps pins.h, PINOUT.md and WIRING.md in step and check_netlist.py checks this sheet.",
        "Analog inputs on ADC1 only. GPIO36/39 carry analog only (they glitch when the ADC runs). Kill lines on pins quiet at boot."], columns=[
        ("ESP32", ["U20", "U21"]),
        ("Pedal", ["R22", "R20", "R21", "C20", "D20", "NOTE:pedal"]),
        ("Wheel speed, temperature, sound", ["R23", "C21", "R24", "C22", "R27", "U31", "R28"]),
    ]),
    dict(file="i2c_io", blurb='2 x ADS1115 current sense, PCF8574 switches\n(shifter, horn, E-stop sense, seat), MPU-6050,\nPCA9685 accessory PWM.', name="I2C bus and switch inputs", parts=i2c, comments=[
        "I2C on GPIO22 (SCL) / GPIO23 (SDA) at 3.3 V. Addresses: 0x20 0x36 0x3C 0x40 (+0x70 all-call) 0x48 0x49 0x68.",
        "Off-box I2C (AS5600 on the steering column, OLED on the dash) is the weak spot: keep runs short, twisted with GND (#36)."], columns=[
        ("Current sense ADCs", ["U22", "U23"]),
        ("Switches", ["U24", "NOTE:pcf"]),
        ("IMU, PWM, pull-ups", ["U26", "U28", "R25", "R26"]),
    ]),
    dict(file="accessories", blurb='MOSFET board for lights and fans, 12 V branch\nfuses F9/F10, music amp, DFPlayer + alert speaker.', name="Accessories: fans, lights, sound", parts=acc, comments=[
        "Phase 7. Nothing here is in the 24 Oct order except the inline fuse holders.",
        "Music has its own amp with Bluetooth on that board only; the ESP32 never runs Wi-Fi or Bluetooth."], columns=[
        ("Switched loads", ["U29", "FAN1", "FAN2"]),
        ("12 V branches", ["F9", "F10", "U32", "#FLG07"]),
        ("Car sounds", ["U30", "NOTE:audio"]),
    ]),
]

NOTES = {
    "going_back": ("Going back to stock", [
        "1. Key off. Unplug J1-J8 from the box.",
        "2. Plug the stock HY2005RX controller into the",
        "   same plugs (motor splitters: unplug, rejoin).",
        "3. Unplug J9-J12; the stock car has none of them.",
        "No stock wire is ever cut (#33)."]),
    "main_path": ("Main current path", [
        "BT1 + -> F1 40 A -> J1 -> K1 stud A -> K1 stud B",
        "-> TB2 -> B+ of U10-U14 (10 AWG).",
        "B- of U10-U14 -> TB1 star -> J1 -> BT1 -.",
        "The blade fuse block carries only the small",
        "branches; it is not in the 40 A path.",
        "K1 precharge: decide after the bench test",
        "(5 x 470 µF behind a 125 A contactor)."]),
    "coil_chain": ("Coil chain", [
        "KEY_24V -> F3 1 A -> J9.1 -> S2 NC1 -> S2 NC2",
        "-> J9.2 -> K1 coil -> Q1 -> GND.",
        "Unplugging J9 opens K1 (fail-safe).",
        "Q1 gate: GPIO12 via R1, R2 pull-down."]),
    "fuses": ("Fuse rule", [
        "Every fuse protects the wire after it:",
        "the wire is rated above the fuse.",
        "F1 40 A: 10 AWG. F2 15 A: 14 AWG.",
        "F6 10 A: 16 AWG. F3-F5, F7, F8: 20 AWG",
        "or heavier. See WIRE_LIST.md."]),
    "rails": ("Rail budget", [
        "5V_LOGIC  U1 LM2596  ESP32, RX, halls, U2   ~0.5 A  F4 2 A",
        "3V3_SENSE U2 AMS1117 IBT-2 logic, I2C       ~0.1 A  (via F4)",
        "5V_ACC    U3 5 A     DFPlayer, USB, LEDs    ≤5 A    F5 3 A",
        "12V_ACC   U4 10-20 A lights F9, amp F10     ≤10 A   F6 10 A",
        "Fuses are on the 24 V input side: 5 A at 5 V",
        "is about 1.2 A at 24 V."], 150.0),
    "pedal": ("Pedal input", [
        "Switch pedal: between V+ and SIG.",
        "Pot: ends on V+ / GND, wiper on SIG.",
        "5 V hall throttle: add a divider (TBD #16).",
        "Shorted high reads as full pedal: open",
        "bench check (CLAUDE.md)."]),
    "pcf": ("PCF8574 inputs", [
        "P0 shift forward, P1 shift reverse, P2 horn,",
        "P3 E-stop sense (NO contact: 0 = pressed),",
        "P4 seat switch. P5-P7 spare.",
        "The E-stop itself cuts K1 in hardware;",
        "P3 only tells the firmware why."]),
    "audio": ("Audio", [
        "DFPlayer on 5V_ACC so speaker current stays",
        "off the ESP32 supply. See docs/hardware/AUDIO.md."]),
}

# ---------------------------------------------------------------- variant B (separate project)

VARIANT_B = [
    Part("U40", M("74LVC08"), "74LVC08", {"1": "FL_RPWM", "2": "DRIVE_EN", "3": "FL_PWM_G", "4": "FR_RPWM", "5": "DRIVE_EN", "6": "FR_PWM_G",
                                         "9": "RL_RPWM", "10": "DRIVE_EN", "8": "RL_PWM_G", "12": "RR_RPWM", "13": "DRIVE_EN", "11": "RR_PWM_G",
                                         "14": "3V3_SENSE", "7": "GND"},
         "74LVC08 quad AND", BUY, "A1", "MDD20A has no enable: DRIVE_EN gates each PWM line so layer 2 survives"),
    C("C40", "100n", "3V3_SENSE", "GND", note="U40 decoupling"),
    R("R40", "10k", "DRIVE_EN", "GND", note="DRIVE_EN pull-down at U40"),
    Part("U41", M("MDD20A"), "MDD20A front", {"GND": "GND", "PWM1": "FL_PWM_G", "DIR1": "FL_LPWM", "PWM2": "FR_PWM_G", "DIR2": "FR_LPWM",
                                             "VB+": "MOTOR_24V", "VB-": "GND", "M1A": "FL_CS_IN", "M1B": "FL_M_NEG", "M2A": "FR_CS_IN", "M2B": "FR_M_NEG"},
         "Cytron MDD20A", BUY, "box", "xx_RPWM becomes PWM and xx_LPWM becomes DIR (firmware change). 30 V max"),
    Part("U42", M("MDD20A"), "MDD20A rear", {"GND": "GND", "PWM1": "RL_PWM_G", "DIR1": "RL_LPWM", "PWM2": "RR_PWM_G", "DIR2": "RR_LPWM",
                                            "VB+": "MOTOR_24V", "VB-": "GND", "M1A": "RL_CS_IN", "M1B": "RL_M_NEG", "M2A": "RR_CS_IN", "M2B": "RR_M_NEG"},
         "Cytron MDD20A", BUY, "box"),
    CP("C41", "470µ 50V", "MOTOR_24V", "GND", "Electrolytic 470 µF ≥35 V low-ESR", "box, on U41 VB+/VB−"),
    CP("C42", "470µ 50V", "MOTOR_24V", "GND", "Electrolytic 470 µF ≥35 V low-ESR", "box, on U42 VB+/VB−"),
] + [
    Part(f"U4{3 + i}", M("Current_Sensor_TBD"), f"CS {c}", {"IP+": f"{c}_CS_IN", "IP-": f"{c}_M_POS", "VCC": "3V3_SENSE", "GND": "GND", "OUT": f"IS_{c}"},
         "Hall current sensor, 3.3 V output (e.g. ACS724 or ACS758 with divider; TBD)", TBD, "box", issue="#32")
    for i, c in enumerate(CH)
] + [PWR("#FLG40", "MOTOR_24V"), PWR("#FLG41", "GND"), PWR("#FLG42", "3V3_SENSE")]

VARIANT_B_SHEET = dict(file="drive_b", name="Drive, variant B (2 x MDD20A)", parts=VARIANT_B, comments=[
    "Replaces sheet 6 (drive, variant A) for the four drive wheels. Steering stays on the U14 IBT-2.",
    "Same ESP32 pins with new meaning: xx_RPWM = PWM, xx_LPWM = DIR. Undecided (#32)."], columns=[
    ("PWM gate", ["U40", "C40", "R40"]),
    ("Drivers", ["U41", "C41", "U42", "C42"]),
    ("Current sensors (TBD)", ["U43", "U44", "U45", "U46"]),
    ("", ["#FLG40", "#FLG41", "#FLG42"]),
])
