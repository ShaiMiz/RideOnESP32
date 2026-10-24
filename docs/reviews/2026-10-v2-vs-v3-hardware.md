# Hardware: old v2 design vs the v3 schematic (v0.3)

24 Oct 2026. Compares the v2 hardware (archive/docs-v2, archive/hardware_reference/Component_Descriptions_and_Purpose.pdf, the v2.1 pin map) with the v3 KiCad schematic v0.3 in [docs/hardware/kicad/](../hardware/kicad/) and its [BOM](../hardware/kicad/BOM.md), which is matched to the AliExpress order of 24 Oct 2026.

## Short answer

- **v2 had one layer of safety: software.** If the ESP32 hung, crashed or got a bad radio packet, nothing in hardware could stop the motors. The motor drivers could be wired permanently enabled, there was no contactor, no E-stop, and the key switch wasn't in any safety path.
- **v3 has three layers**, and a failure in a higher one never removes a lower one: (1) a power chain a person can always cut with no software (fuse, key, E-stop, contactor), (2) drivers that default to off (pulled-down enable lines at every board), (3) the ESP32 on top, which can open the contactor but never close it.
- **Power distribution was rebuilt.** v2 put a 30 A breaker and a small low-voltage relay board in series with a ~33 A motor load on 12-14 AWG wire, and fed the ESP32 3.3 V into its 5 V pin. v3 has a 40 A fuse at the battery, a 125 A contactor, 10 AWG for the motor path, a fuse on every branch sized to its wire, and separate logic and accessory rails.
- **Sensing moved off the ESP32's pins onto I2C**, which ended v2's shared, mode-switched pins: per-motor current on two ADS1115s, switches on a PCF8574, steering angle on an AS5600.
- **Kept from v2:** the ESP32, four BTS7960 boards with one per wheel (RPWM/LPWM control), the CRSF/ELRS radio, the LM2596, the battery-divider idea and the parent's T-Lite radio.
- **Still open in v3:** the car-facing plugs (#17), the battery and motor labels, the box size, and four hardware gaps from the safety review (roll-away when killed, single-motor stall backstop, box fire rating, EMC). Details at the end.

## Side by side

🛡 = mainly a safety change.

| Area | v2 | v3 (schematic v0.3) | Why |
| --- | --- | --- | --- |
| Main protection 🛡 | 30 A resettable breaker, then XH-M609 low-voltage cut-off board, both in series with all four motors | F1 40 A ANL fuse within 15 cm of battery +, then K1 125 A contactor | A 30 A breaker trips on the ~33 A full load v2's own docs predict, and resets itself after a short. A small relay board carries motor current it isn't built for. A fuse at the terminal protects the whole cable; the contactor is rated far above the load |
| Hard stop 🛡 | None. "Emergency stop" was a firmware function | Latching mushroom E-stop S2 with **both NC blocks in series** in the K1 coil, plus the key switch in the same chain | Stopping must not depend on the code running. Two NC blocks in series: one welded block still leaves the other to open. Unplugging the E-stop cable also opens K1 |
| Contactor control 🛡 | - | Q1 IRLZ44N in the coil's low side, gate on GPIO12 with a 10k pull-down | The ESP32 can drop K1 on a fault, but can never close it against the key or E-stop. Reset, crash or brown-out opens it |
| Driver enable 🛡 | Guide allowed R_EN/L_EN tied to VCC ("always enabled") or to GPIO21 | DRIVE_EN (GPIO21) and STEER_EN (GPIO2) with a 10k pull-down **at every board** (5 resistors) | Tied to VCC, the drivers follow whatever the PWM pins do during boot, flashing or a crash. A pull-down per board keeps a board off even when its enable wire breaks |
| Strapping pins 🛡 | RL_RPWM on GPIO12 ("if boot problems, change it") | GPIO12 and GPIO2 carry kill lines that want to be LOW at boot anyway; GPIO5/15 carry signals that idle HIGH. Build fails on a bad map (pin_rules.h) | A PWM on GPIO12 can stop the board booting, and a motor pin that glitches at boot is a motor that twitches |
| Shared pins | GPIO34 = pedal or FR current, GPIO39 = shifter or FL current, switched by mode | Every GPIO has one job; extra I/O goes on I2C | Mode-switched pins read the wrong thing whenever the mode logic is wrong |
| ESP32 power | 3.3 V from an LM2596 into VIN | LM2596 at 5.0 V into VIN; separate 3.3 V regulator (AMS1117) for sensors and driver logic | VIN feeds the on-board 3.3 V regulator and needs 5 V; 3.3 V there leaves the chip under-volted. Sensors on their own rail keep their noise off the ESP32 |
| Rails | LM2596 3.3 V + LM2596 12 V, both straight from the battery side | 5V_LOGIC, 3V3_SENSE, 5V_ACC, 12V_ACC; logic and accessories never share a converter; all fed from the key side, before K1 | Key off turns everything off; an E-stop leaves the ESP32 alive to report why it stopped |
| Fusing per branch 🛡 | One breaker for everything | F2 key 15 A, F3 coil 1 A, F4-F8 rails, fans and divider, F9/F10 on the 12 V outputs; each fuse protects the wire after it | A short on a thin signal-size wire would otherwise wait for a 30-40 A device and burn the wire first |
| Wire gauge 🛡 | 12-14 AWG main line | 10 AWG battery, contactor and driver B+/B−; 12 AWG motor leads; 14-22 AWG by fuse ([wire list](../hardware/kicad/WIRE_LIST.md)) | 14 AWG at 33 A runs hot. Every run now has a gauge, a colour and the fuse that protects it |
| Grounding | "All grounds common" | One ground star TB1; logic ground reaches it by one wire from the interface board | Avoids motor current flowing through signal grounds and upsetting the ADC and I2C |
| Current sensing | One ACS712 (5 V output into a 3.3 V ADC through a divider) on the battery lead, optional; BTS7960 IS pins on spare ADC pins in some modes | Every driver's IS line (R_IS + L_IS) → 4.7k → ADS1115 with 10 nF filter and a 3.3 V clamp; 5 channels | Per-motor current gives per-wheel overcurrent, stall and driver-fault detection (the IS line pinned high is the BTS7960's fault flag). The clamp protects the ADC from the 4-7 mA fault current |
| Thermal | Diode-OR of a "FAULT" pin into GPIO19 | DS18B20 on the hottest heatsink, box fans | The BTS7960 has no FAULT pin; the v2 circuit sensed nothing. Driver faults now show on the IS lines |
| Battery sense | 100k / 12k on GPIO35: 3.21 V at 30 V | 150k / 12k with 100 nF on GPIO39: 2.2 V at 29.4 V, fused at the source | v2's divider ran into the top of the ESP32 ADC, where it is non-linear, right at full charge. GPIO35 is now a wheel-speed input |
| Pedal | Pot from 3.3 V straight into GPIO34 | 100 Ω supply resistor, 10k series, 100k pull-down, 100 nF, clamp, into GPIO36 | A pedal wire runs through the car; a short or a stray 24 V must not kill the ADC or the sensor rail, and an unplugged pedal must read "released" |
| Shifter, horn, seat | GPIO39 and GPIO5 with internal pull-ups (GPIO39 has none) | PCF8574 inputs; E-stop state on P3 | GPIO34-39 have no internal pull-ups, so v2's shifter input floated. Strapping pin GPIO5 is now the DFPlayer TX |
| Steering | GPIO2 DIR + GPIO15 PWM to an unspecified driver, not implemented | Fifth IBT-2 (U14) on GPIO18/19 with its own enable, IS sense, AS5600 angle sensor | Same driver type and safety treatment as the wheels; the angle sensor closes the loop |
| Wheel speed | - | Two A3144 halls on GPIO34/35 with external pull-ups | For traction control and speed caps; input-only pins chosen so the ADC glitch on 36/39 can't add ticks |
| Radio | ELRS receiver on 3.3 V or 5 V (via a 5 V regulator off the 12 V rail) | AION RX Mini on 5V_LOGIC, CRSF on UART2 | Same protocol; the receiver now sits on the logic rail, not behind the accessory converter |
| Lights | IRF540N (not logic-level) or a relay from GPIO22 | PCA9685 → 4-channel logic-level MOSFET board, low side, F9 fused | An IRF540N barely turns on from 3.3 V and runs hot |
| Connectors | XT60s, screw terminals | Box edge J1-J8 mate the stock plugs (plug and play, TBD #17); our own sealed Deutsch DT plugs J9-J12; ferrules on every screw terminal | The box swaps with the stock controller and the car can go back to stock without cutting a wire |
| Enclosure | - | Flame-retardant (UL94 V-0) or metal box, fans, glands (size TBD #18) | Safety review gap 13: a plain ABS box near a 40 A circuit |

## Parts: removed, kept, added

### Removed from v2

| Part | Why |
| --- | --- |
| 30 A resettable circuit breaker | Below the full load, self-resetting after a fault; replaced by a 40 A ANL fuse at the battery |
| XH-M609 low-voltage cut-off board | Its relay isn't built for motor current (check its label); low-battery cut-off is now firmware (21 V with hysteresis) acting through DRIVE_EN and K1 |
| ACS712-30B current sensor | 5 V output into a 3.3 V ADC, one sensor for four motors; replaced by the drivers' own IS outputs on ADS1115s |
| Diode-OR thermal "FAULT" circuit | Sensed a pin that doesn't exist |
| IRF540N lights MOSFET / relay | Not logic-level; replaced by a logic-level MOSFET board |
| Second LM2596 (3.3 V) | ESP32 takes 5 V; 3.3 V for sensors now comes from an AMS1117 |
| HC-12 433 MHz radio, MCP23017 | Second, unsecured radio path; no UART left for it. The MCP23017 libraries were never used |

### Kept

| Part | Notes |
| --- | --- |
| ESP32-WROOM-32D, 30-pin | Same board (v2 docs said WROOM-32U); pins re-audited (PR #4) |
| 4 × BTS7960 (IBT-2) | One per wheel, RPWM/LPWM, now with enable pull-downs, IS sensing and 470 µF at each board |
| LM2596 | Now set to 5.0 V for the logic rail |
| ELRS receiver + Jumper T-Lite | CRSF, link-loss judged by link state (firmware) |
| Battery divider | New values, filter and fuse |
| 24 V battery and the four motors | Stock; labels still to read (#13, #14) |

### Added in v3

Power and safety: F1 ANL 40 A, K1 125 A contactor, S2 2NO+2NC E-stop, Q1 IRLZ44N, D1 flyback, fuse block with F3-F8, inline F2/F9/F10, ground star TB1 and motor bus TB2, D2 TVS (not fitted until the battery voltage is known). Drive: fifth IBT-2 for steering, 470 µF per board, IS networks. Sensing and I/O: 2 × ADS1115, PCF8574, AS5600, MPU-6050 (owned), DS18B20, 2 × A3144, seat switch. Rails: AMS1117, 5 V and 12 V accessory bucks. Accessories: PCA9685, MOSFET board, two fans, DFPlayer and speaker, ZK-502T music amp. Build: interface board A1, Deutsch DT plugs, 10/12 AWG wire, lugs, ferrules, heat shrink.

Of these, the 24 Oct order covers the power and safety chain, the sensing ICs, the passives and the heavy wire; see [BOM.md](../hardware/kicad/BOM.md) for line-by-line status and the three things to check when it arrives (capacitor voltage, ADS1115 quantity, fuse values).

## What v3 still doesn't do (hardware)

From the [safety-standards review](2026-10-safety-standards.md); none is fixed by this schematic:

1. **Roll-away when killed.** Dropping DRIVE_EN or opening K1 leaves the motors free-wheeling, so the car can roll on a slope. Fixing it needs a brake that holds without power (or motor shorting through a normally-closed contact); not drawn.
2. **Single-motor stall backstop.** One stalled motor likely draws less than the 40 A fuse and the 20 A software limit. Per-motor fuses or a lower firmware stall limit, then a stall test in commissioning.
3. **Box fire rating.** Specified as UL94 V-0 or metal in the BOM; not bought.
4. **EMC.** Motor caps C201-C205, twisted pairs and the ground star help; nothing is tested.

The regen spike when K1 opens on a rolling car (D2) and the K1 precharge are also open, waiting on the battery label and the bench test.
