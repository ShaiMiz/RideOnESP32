# Fault codes

Each code is the short name the serial log prints. Later the radio screen and the OLED will show it too. Faults are defined in `lib/rideon_core/src/Faults.h`.

Every fault **latches**. It stays set after its cause goes away, until the parent re-arms from Locked with the sticks centred and the pedal released (see [CONTROL_MODES.md](CONTROL_MODES.md)). A fault whose cause is still present cannot be cleared.

| Code | Cause | Stop | Clears | First thing to check |
| --- | --- | --- | --- | --- |
| `LINK` | No RC packet for 200 ms, link quality under 40 %, or the receiver's link flag down | Brake | Re-arm after the link returns | Radio on? Receiver powered (5 V)? TX/RX on GPIO16/17? Bound with the same phrase? |
| `KILL` | Kill switch (CH5) not at run, or not readable | Coast, contactor open | Kill switch to run, then re-arm | Switch position; CH5 mix |
| `BATT_LOW` | Battery under 21.0 V for more than 2 s | Brake | Re-arm once the battery rests above 22.5 V | Charge the battery |
| `BATT_SENS` | Battery reading under 5 V or over 32 V | Brake | Re-arm once the reading is plausible | Divider on GPIO39 / VN (150k/12k), loose wire |
| `OVERCUR` | One motor over 20 A, or all four over 35 A, for 300 ms | Coast | Re-arm | Stalled wheel, jammed gearbox, short in motor wiring. Check `IS_RESISTOR_OHMS` |
| `DRIVER` | A BTS7960 IS line at the 3.3 V clamp: the driver reports over-temperature, short or under-voltage | Coast (also resets the driver's latch) | Re-arm once the driver cools | Heatsink temperature, motor short, driver supply |
| `INPUT` | Throttle, steering or kill channel out of range, or the switch expander not answering in Kid mode | Brake | Re-arm | Radio channel setup; PCF8574 wiring at 0x20 |
| `TILT` | Car tipped past the limit (later, MPU-6050) | Coast | Re-arm once level | |
| `ESTOP` | The E-stop sense input (PCF8574 P3) reads pressed. The E-stop already cut the contactor coil in hardware, but the ESP32 stays powered | Coast, contactor held open | Release the E-stop, then re-arm. Releasing alone never restarts the car | E-stop contact block wiring to P3; it reads pressed when pulled to GND |
| `SETUP` | A driver, PWM channel or ADS1115 failed to start at boot | Contactor open | Reboot only | Serial log at boot; I2C wiring |

The serial log also prints the **reset reason** at every boot. `WATCHDOG`, `BROWNOUT` or `PANIC` means the ESP32 restarted unexpectedly. The pull-downs keep the car stopped through the restart, but find the cause before driving again.
