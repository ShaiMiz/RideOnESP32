# Wiring

Every connection between the ESP32 and the rest of the car. The pin rows are copied from [include/pins.h](../../include/pins.h). If you change a pin, change pins.h, this table and [PINOUT.md](PINOUT.md); CI checks that all three agree.

## ESP32 (30-pin, on the MRD068A breakout)

Pin-by-pin reasons, boot behaviour and the header position of each pin are in [PINOUT.md](PINOUT.md).

| GPIO | Connects to | Notes |
| --- | --- | --- |
| 25 / 26 | Front-left IBT-2 RPWM / LPWM | |
| 27 / 14 | Front-right IBT-2 RPWM / LPWM | GPIO14 toggles at boot; DRIVE_EN keeps the driver off |
| 32 / 33 | Rear-left IBT-2 RPWM / LPWM | |
| 13 / 4 | Rear-right IBT-2 RPWM / LPWM | |
| 18 / 19 | Steering IBT-2 RPWM / LPWM | |
| 21 | R_EN and L_EN of all four drive boards | 10k pull-down to GND at the drivers |
| 2 | R_EN and L_EN of the steering board | Strapping pin, must be LOW at boot; 10k pull-down (on-board LED also pulls low) |
| 12 | Contactor MOSFET gate | Strapping pin, must be LOW at boot; 10k pull-down |
| 16 | Receiver TX | CRSF, UART2, 420000 baud |
| 17 | Receiver RX | |
| 22 | I2C SCL | All I2C devices |
| 23 | I2C SDA | |
| 36 (VP) | Pedal | ADC1; input only |
| 39 (VN) | Battery divider (150k from battery +, 12k to GND) | ADC1; 2.2 V at 29.4 V |
| 34 | Wheel-speed hall, left | Input only; external 10k pull-up |
| 35 | Wheel-speed hall, right | Input only; external 10k pull-up |
| 5 | Sound module RX | Serial TX; strapping pin, idles HIGH |
| 15 | DS18B20 data | 1-Wire, 4.7k pull-up keeps the strapping pin HIGH |
| 1 / 3 | Nothing | USB serial: flashing and logs. TX0 also feeds the listen-only Bluetooth module through 1k ([BLUETOOTH.md](BLUETOOTH.md)) |
| VIN | 5 V logic rail | Not 3.3 V |
| 3V3 | Nothing | |
| GND | Star ground | |

## I2C bus (GPIO22 SCL, GPIO23 SDA, 3.3 V)

| Address | Device | Job |
| --- | --- | --- |
| 0x48 | ADS1115 #1 (ADDR to GND) | A0 FL, A1 FR, A2 RL, A3 RR current sense |
| 0x49 | ADS1115 #2 (ADDR to VDD) | A0 steering current sense; A2 motor bus (MOTOR_24V through 150k / 12k); A1 kept for AS5600 OUT, A3 spare |
| 0x20 | PCF8574 | P0 shift forward, P1 shift reverse, P2 horn, P3 E-stop sense, P4 seat switch. Switches pull to GND |
| 0x36 | AS5600 | Steering angle (phase 5) |
| 0x68 | MPU-6050 | Tilt and crash (later) |
| 0x3C | SSD1306 OLED 0.91" | Dash (later) |
| 0x40 | PCA9685 | Channel 0 lights (later), channel 1 box fans (full on whenever the ESP32 runs) |

## Each IBT-2 (BTS7960) board

| IBT-2 pin | Connects to |
| --- | --- |
| VCC | 3.3 V sensor rail, so the ESP32's 3.3 V signals read as a clean HIGH |
| GND | Logic ground |
| RPWM / LPWM | ESP32 PWM pins above |
| R_EN + L_EN | Joined, to GPIO21 (drive boards) or GPIO2 (steering) |
| R_IS + L_IS | Joined into one sense line ─► 4.7k series resistor ─► ADS1115 input, with a clamp diode to 3.3 V |
| B+ / B− | Switched 24 V after the contactor, 10 AWG, 470 µF 35 V+ capacitor at the board |
| M+ / M− | Motor |

Never tie R_EN or L_EN to VCC. Only one IS side conducts at a time, which is why joining them works. The board's two IS resistors are then in parallel, so the firmware divides by half of one (`IS_SENSE_OHMS`).

## Receiver (Jumper AION RX Mini)

| Receiver | ESP32 |
| --- | --- |
| 5V | 5 V logic rail |
| GND | GND |
| TX | GPIO16 |
| RX | GPIO17 |

Wiring photos go in [photos/](photos/), and the schematic in [kicad/](kicad/) (with [CONNECTORS.md](kicad/CONNECTORS.md) and [WIRE_LIST.md](kicad/WIRE_LIST.md)).
