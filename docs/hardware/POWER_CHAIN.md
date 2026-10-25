# Power and safety chain

There are three layers, and a failure in a higher layer never removes a lower one:

1. A power chain a person can always cut, with no software involved.
2. Drivers that default to off.
3. An ESP32 that only adds control on top.

## Motor power (no software)

```
Battery + ─► 40 A fuse (within 15 cm of the terminal) ─► fuse block ─┬─► contactor ─► 4 + 1 BTS7960 B+ (470 µF 35 V+ cap at each)
                                                                     │
                                                                     └─► key switch ─► E-stop (NC, latching) ─► contactor coil ─► MOSFET (gate: GPIO12, 10k pull-down) ─► GND
```

- **Fuse:** 40 A (ANL or maxi blade), matching the stock controller's rating.
- **Key switch** and **red latching mushroom E-stop** are in series with the contactor coil. Either one opens the contactor whatever the ESP32 does.
- **Contactor:** 24 V coil, or an 80 A+ automotive relay, with a flyback diode across the coil.
- **MOSFET:** a logic-level N-MOSFET (e.g. IRLZ44N module) in series with the coil, gate on GPIO12 with a 10k pull-down. The ESP32 can open the contactor but can never close it against the key or E-stop. A reset, crash or brown-out opens it. GPIO12 is a strapping pin and must be LOW at boot, which the pull-down also guarantees.
- **Wire:** 10 AWG red and black for the motor path. XT60s on the battery and motors so they can be unplugged.

## Low-voltage rails

All four rails are fed from the key side of the fuse block, **before** the contactor. Lights, sound and the ESP32 stay powered after an E-stop, so the ESP32 can report why it stopped. Logic and accessories never share a converter.

| Rail | Converter | Feeds | Budget | Fuse |
| --- | --- | --- | --- | --- |
| 5 V logic | LM2596 set to 5.0 V | ESP32 VIN, receiver, OLED | about 0.5 A | 2 A |
| 3.3 V sensors | 1 A regulator from 5 V logic | IBT-2 logic (VCC), ADS1115 ×2, AS5600, MPU-6050, PCF8574 | about 0.1 A | shared |
| 5 V accessories | 24 to 5 V buck, 5 A | USB, LED strips, fans | up to 5 A | 7.5 A |
| 12 V accessories | 24 to 12 V buck, 10 to 20 A, sealed | Lights, audio amp | up to 10 A | 15 A |

Do **not** feed 3.3 V into the ESP32's VIN pin, as the old docs suggested. VIN takes 5 V.

## Driver enable lines

| Line | Pin | Pull-down | Drops when |
| --- | --- | --- | --- |
| Drive enable (R_EN + L_EN of all four drive boards) | GPIO21 | 10k at the drivers | Kill switch, hard fault, boot, reset, crash; also after braking to a stop in Locked |
| Steering enable | GPIO2 | on-board, plus 10k | Every stop, and always in Kid mode |
| Contactor hold | GPIO12 | 10k | Kill switch, E-stop, driver fault, over-current, setup failure, boot, reset, crash; and at boot until the weld check passes |

Run each enable as a short twisted pair with GND, star-routed from the ESP32.

## Contactor feedback

The ESP32 reads the motor bus (after the contactor) through a 150k / 12k divider on ADS1115 #2 A2, and a 2.2k 3 W bleed resistor drains the driver capacitors while the contactor is open (5 s time constant).

- **Boot weld check:** the contactor stays open until the bus has drained below 30 % of the battery. Still above that after 10 s means the contactor is welded: fault `K1_WELD`, reboot only. A welded contactor would leave the drivers powered through an E-stop.
- **Bus missing:** the contactor held closed for 300 ms with the bus under 80 % of the battery is fault `NO_BUS`. That also catches an E-stop press when its sense wire is broken.
- **No precharge resistor.** One across the contactor would keep the bus live with the contactor open, which hides a weld and partly defeats the E-stop. The inrush into 5 x 470 µF at key-on is about 1 J; watch the contactor tips for pitting during commissioning.

## Bench test (phase 3)

With a lamp or one motor as the load and the ESP32 **unplugged**: the key and the E-stop must each cut the load. With the ESP32 plugged in, pressing its reset button must open the contactor.
