# Fault codes

Each code is the short name the serial log prints. Later the radio screen and the OLED will show it too. Faults are defined in `lib/rideon_core/src/Faults.h`.

Every fault **latches**. It stays set after its cause goes away, until the parent re-arms from Locked with the sticks centred and the pedal released (see [CONTROL_MODES.md](CONTROL_MODES.md)). A fault whose cause is still present cannot be cleared.

| Code | Cause | Stop | Clears | First thing to check |
| --- | --- | --- | --- | --- |
| `LINK` | No RC packet for 200 ms, link quality under 40 %, or the receiver's link flag down | Brake | Re-arm after the link returns | Radio on? Receiver powered (5 V)? TX/RX on GPIO16/17? Bound with the same phrase? |
| `KILL` | Kill switch (CH5) not at run, or not readable | Coast, contactor open | Kill switch to run, then re-arm | Switch position; CH5 mix |
| `BATT_LOW` | Battery under 21.0 V for more than 2 s | Brake | Re-arm once the battery rests above 22.5 V | Charge the battery |
| `BATT_SENS` | Battery reading under 5 V or over 32 V | Brake | Re-arm once the reading is plausible | Divider on GPIO39 / VN (150k/12k), loose wire |
| `OVERCUR` | One motor over 20 A, or all four over 35 A, for 300 ms | Coast, contactor open | Re-arm | Stalled wheel, jammed gearbox, short in motor wiring. Check `IS_PIN_RESISTOR_OHMS` |
| `DRIVER` | A BTS7960 IS line (a drive wheel or the steering board) above `IS_FAULT_VOLTS` (1.8 V with 1k boards) for 300 ms: the driver reports over-temperature, short or under-voltage | Coast (also resets the driver's latch), contactor open, since a shorted MOSFET ignores the enable | Re-arm once the driver cools | Heatsink temperature, motor short, driver supply |
| `INPUT` | Throttle, steering or kill channel out of range, or the switch expander not answering in Kid mode | Brake | Re-arm | Radio channel setup; PCF8574 wiring at 0x20 |
| `TILT` | Car tipped past the limit (later, MPU-6050) | Coast | Re-arm once level | |
| `ESTOP` | The E-stop sense input (PCF8574 P3) reads pressed. The E-stop already cut the contactor coil in hardware, but the ESP32 stays powered | Coast, contactor held open | Release the E-stop, then re-arm. Releasing alone never restarts the car | E-stop contact block wiring to P3; it reads pressed when pulled to GND |
| `CUR_SENS` | A current-sense or motor-bus channel (ADS1115) has not given a good reading for 250 ms: I2C bus stuck, an ADS1115 unplugged or not answering | Brake | Re-arm once readings flow again | I2C wiring at 0x48 / 0x49, ADS1115 power, bus length |
| `NO_BUS` | No motor bus (MOTOR_24V on ADS1115 #2 A2). Either the boot weld check is still waiting for the bus to drain below 30 % of the battery (up to 10 s after a quick key cycle), or the contactor was commanded closed for 300 ms and the bus stayed under 80 % of the battery | Coast | Re-arm once the bus is up | Normal for a few seconds after key-on. Otherwise: F3 coil fuse, J9 and the E-stop (pressed with a broken sense wire?), K1 coil, Q1 |
| `K1_WELD` | At boot, with the contactor held open, the motor bus stayed above 30 % of the battery for 10 s: the contactor is welded shut, or something feeds the bus around it. The E-stop no longer isolates the drivers | Coast, contactor commanded open | Reboot only (the reboot repeats the check) | Key off, measure across K1; replace it. Check the R7 bleed resistor is fitted |
| `HOT` | Box above 65 °C for 5 s (DS18B20); clears only below 55 °C. Once `BOX_TEMP_REQUIRED` is set, also a lost sensor reading | Brake | Re-arm once cooled | Fans running (log `fan=1`)? Filter clogged? Driver heatsinks |
| `SETUP` | A driver, PWM channel or ADS1115 failed to start at boot | Contactor open | Reboot only | Serial log at boot; I2C wiring |

Each trip is also logged with the part it came from (`comp=FL`) and counted in the fault table; see [LOG_EVENTS.md](LOG_EVENTS.md).

The serial log also prints the **reset reason** at every boot, in the `BOOT` record. `WATCHDOG`, `BROWNOUT` or `PANIC` means the ESP32 restarted unexpectedly. The pull-downs keep the car stopped through the restart, but find the cause before driving again.
