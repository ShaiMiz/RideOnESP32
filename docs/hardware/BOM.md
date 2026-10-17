# Bill of materials

From the gear photos (Desktop\ss, 17 Oct 2026) and the hardware plan.

## Owned

| Part | Qty | Use |
| --- | --- | --- |
| Jumper T-Lite V2 ELRS radio | 1 | Parent's radio. Needs an 18650 cell |
| Jumper AION RX Mini ELRS receiver | 1 | On the car, 5 V |
| ESP32-WROOM-32D dev board, 30-pin, USB-C | 2 | Controller and a spare |
| MRD068A screw-terminal breakout | 1 | Vibration-proof connections |
| BTS7960 (IBT-2) 43 A driver | 4 | One per drive wheel |
| HY2005RX-24V/40A stock controller | 1 | Plan B: puts the car back to stock |
| LM2596 buck converter | 1+ | 5 V logic rail |
| MPU-6050 (GY-521) IMU | 1 | Tilt and crash cut-out (later) |
| 0.91" SSD1306 OLED | 1 | Dash (later) |
| Ride-on key switch | 1 | Master key in the contactor coil circuit |
| XT60 pairs | 5 | Battery and motor connectors |
| 10 AWG red wire, 5 m | 1 | Motor power |
| 555 24 V motors (loose) | 3 + 1 | Spares, if they match the car's motors |

## To buy

| Part | Qty | Why |
| --- | --- | --- |
| 40 A fuse and inline holder | 1 | Battery short protection |
| 24 V contactor or 80 A+ relay, flyback diode | 1 | Hard power cut |
| Latching red mushroom E-stop | 1 to 2 | One on the car, optionally one at the back for a walking adult |
| Logic-level N-MOSFET module (IRLZ44N) | 1 | Contactor hold |
| 10k, 4.7k resistors; 3.3 V clamp diodes | pack | Pull-downs and sense-line protection |
| BTS7960 (IBT-2) | 1 | Steering motor |
| 470 µF 35 V+ low-ESR capacitors | 5 | One per driver |
| 24 to 12 V buck, 10 to 20 A, sealed | 1 | 12 V accessory rail |
| 24 to 5 V buck, 5 A | 1 | 5 V accessory rail |
| 3.3 V regulator module, 1 A | 1 | Sensor rail |
| Blade fuse block, 6 to 8 ways, plus fuses | 1 | A fuse per rail |
| ADS1115 ADC module | 2 | Current sensing |
| PCF8574 I/O expander | 1 | Shifter, horn, E-stop sense |
| AS5600 angle sensor with magnet | 1 | Steering position |
| PCA9685 board plus a 4 to 8 channel MOSFET board | 1 each | Accessories |
| A3144 hall sensors and magnets | 2 to 4 | Wheel speed |
| 10 AWG black wire, 5 m | 1 | Ground return |
| 18650 cell, protected | 1 | For the T-Lite |
| DFPlayer Mini and 3" 4 Ω full-range speaker | 1 | Engine and horn sounds |

### Enclosure and wiring (added 17 Oct 2026)

| Part | Qty | Why |
| --- | --- | --- |
| ABS project box with lid, about 250 x 200 x 100 mm, IP54 or better | 1 | Holds the ESP32, receiver, bucks, fuse block and the five BTS7960s, like the stock controller's box |
| 24 V 80 mm fan with finger guard and dust filter | 2 | One pulls air in, one pushes it out across the driver heatsinks |
| DS18B20 waterproof temperature probe | 1 | Lets the ESP32 run the fans and cut power if the box overheats |
| Cable glands, PG9 and PG13.5 | 10 | Seal the motor, battery and sensor cables where they enter the box |
| Nylon M3 standoffs and a mounting plate | kit | Boards mounted off the floor, nothing loose |
| 0.1 µF ceramic capacitors | 10 | One across each motor's terminals for noise |
| 12 or 14 AWG wire, red and black | 10 m each | Driver to motor runs (the 10 AWG is for the battery feed) |
| Wire ferrules, heat shrink, crimp kit | kit | Clean screw-terminal joints that survive vibration |
| Locking JST-XH or Molex connectors | kit | Sensor and signal plugs that can't shake loose |
| 22 AWG silicone wire, assorted colours | 1 set | Signal wiring |

## Not needed

HC-12 433 MHz module (a second, unsecured radio path), BMP280 barometers, DDT-S30 tweeters (they only play above 4 kHz), and the MCP23017 library.
