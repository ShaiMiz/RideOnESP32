# KiCad schematic (v0.3)

The full wiring schematic of the v3 car in KiCad 9, with the tables that go with it. It replaces the SVG draft v0.2 in [../schematic/](../schematic/), which stays for reference.

**Draft.** Drawn before the stock plugs, the motor and battery labels and the box size are known (#13-#18). The car-facing plugs J1-J8 are TBD; don't build that side from it yet.

| File | What it is |
| --- | --- |
| [rideonesp32.pdf](rideonesp32.pdf) | All 9 pages as a PDF: start here |
| `rideonesp32.kicad_pro` | The KiCad project (open with KiCad 9 or later) |
| [CONNECTORS.md](CONNECTORS.md) | Every box-edge plug, pin by pin: net, gauge, colour, fuse, and what each pin meets inside the box and in the car |
| [WIRE_LIST.md](WIRE_LIST.md) / `.csv` | All 227 wires: from, to, net, AWG, colour, protecting fuse, route; the rules and the colour code |
| [BOM.md](BOM.md) / `.csv` | Every part, matched line by line to the 24 Oct AliExpress order, plus owned, to buy, TBD and stock |
| [NETS.md](NETS.md) | Every net and every pin on it |
| [variant_b/](variant_b/) | Drive variant B (2 × MDD20A + 74LVC08 + 4 current sensors), a delta sheet that replaces sheet 6 (#32) |
| [../../reviews/2026-10-v2-vs-v3-hardware.md](../../reviews/2026-10-v2-vs-v3-hardware.md) | What changed from the old v2 hardware, and why |

## Sheets

| Page | Sheet | What it shows |
| --- | --- | --- |
| 1 | Index | Sheet map, how to read it, the three safety layers, open items, revisions |
| 2 | System wiring overview | Every assembly on one A2 page joined by real wires: battery to motors, the safety chain, rails, ESP32, drivers and the I2C chain. A drawing only (lines are bundles); the nets are on pages 3-9 |
| 3 | Car harness and box edge | Everything outside the box: battery and F1, key, pedal, shifter, motors, E-stop, sensors, dash, speakers, and the plugs J1-J12 |
| 4 | Power and safety chain | K1 contactor, coil chain (F3, E-stop, Q1), F2 key feed, fuse block F3-F8, ground star TB1, motor bus TB2, battery divider, motor-bus sense and bleed (R5-R7), D2 TVS |
| 5 | Low-voltage rails | U1 5V_LOGIC, U2 3V3_SENSE, U3 5V_ACC, U4 12V_ACC |
| 6 | Drive, variant A | U10-U14 IBT-2, 470 µF each, enable pull-downs, IS sense networks |
| 7 | Controller and inputs | ESP32 (pins from include/pins.h), receiver, pedal input, wheel-speed pull-ups, DS18B20, sound TX |
| 8 | I2C bus and switch inputs | 2 × ADS1115, PCF8574, MPU-6050, PCA9685, I2C pull-ups |
| 9 | Accessories | MOSFET board, fans, 12 V branch fuses, music amp, DFPlayer |

## How it's made and checked

The schematic is generated, like v0.2, so the drawing, the tables and the pin map can't drift apart:

- [gen/design.py](gen/design.py) holds every part, pin and net; [gen/wiring.py](gen/wiring.py) every physical wire. **Edit those, never the .kicad_sch files or the tables.**
- `python3 gen/generate.py` writes the KiCad files and the tables. The ESP32's pin nets are read from [include/pins.h](../../../include/pins.h), so a pin change shows up here on the next run. It stops if a net has one pin, if a wire joins two different nets, or if any pin has no wire and no stated reason. CI runs it and fails if the committed files are out of date.
- [check.sh](check.sh) then runs KiCad's own ERC (0 errors, 0 warnings in v0.3), exports KiCad's netlist and checks it against design.py and pins.h with [gen/check_netlist.py](gen/check_netlist.py), and writes the PDFs. It uses the official `kicad/kicad:9.0` Docker image, or a local KiCad with `KICAD_CLI=kicad-cli`.

Style: every pin has a short stub and a net label, so each pin says where it goes. Flag-shaped labels are nets used on more than one sheet. Each part carries `Part`, `Source` (owned, ordered 24 Oct, to buy, TBD, stock car), `Location` (box, A1, car, dash) and `Note` fields, which the BOM comes from. Footprints are left empty: there is no PCB; the box is wired from modules on screw terminals plus one small interface board A1.

## Changes from draft v0.2

Kept: every pin, net and part of v0.2 and its 10 deliberate differences from WIRING.md and POWER_CHAIN.md (#31 still tracks reconciling those docs). Changed or added:

1. **E-stop: both NC blocks in series** in the K1 coil (v0.2 used one). One welded block still leaves the other to open.
2. **Enable pull-downs at every driver** (R111-R115, one per board, inside its control plug) instead of one per line, so a broken enable wire leaves that board off.
3. **Pedal:** supply through R22 100 Ω so a shorted pedal cable can't pull down 3V3_SENSE; the 100k pull-down moved to the pedal side of the 10k so an unplugged pedal reads exactly 0.
4. **I2C pull-ups R25/R26 not fitted by default:** seven modules with their own pull-ups already give about 1-2 kΩ. Fit them only if the bus measures above 4.7 kΩ.
5. **Fuses:** F8 1 A (was 0.5 A, to use the ordered values); new F9 3 A and F10 10 A on the 12 V outputs, so the lights and amp wires are protected from a 10-20 A buck.
6. **D2:** a TVS across MOTOR_24V, not fitted until the battery voltage is known, for the regen spike when K1 opens on a rolling car.
7. **Explicit wiring parts:** ground star TB1, motor bus TB2 and the interface board A1 (Q1, sense networks, pull-ups, divider), so the wire list has real end points.
8. **Plugs:** J6/J7 are 4-pin (one pair per motor), J9 carries the E-stop sense pair too, J10 adds the seat switch, J11 the alert speaker, and J12 (music speakers) is new. J9-J12 are Deutsch DT (or clone), sealed.
9. **Ties:** AS5600 DIR to GND, PCA9685 OE to GND, ADS1115/MPU-6050 address pins drawn.
10. **Music path drawn:** ZK-502T amp and speakers (AUDIO.md).

Reference designators were renumbered to plain KiCad style:

| v0.2 | v0.3 |
| --- | --- |
| R10A-R14A, C10A-C14A, D10-D14 (IS networks) | R101-R105, C101-C105, D101-D105 |
| C10B-C14B (470 µF) | C111-C115 |
| R10B, R14B (enable pull-downs) | R111-R115 (one per board) |
| R22/C22, R23/C23 (hall pull-ups) | R23/C21, R24/C22 |
| R24, R25 (I2C pull-ups) | R25, R26 |
| (unnamed) DS18B20 pull-up, DFPlayer series resistor | R27, R28 |
| Variant B R40, U40-U42 | Same, plus C40-C42 and current sensors U43-U46 |
