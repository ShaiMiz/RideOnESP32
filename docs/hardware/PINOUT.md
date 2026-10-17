# ESP32 pinout

Every header pin on our board, what it does in this project, and why it sits where it does. The pin numbers come from [include/pins.h](../../include/pins.h); `tools/check_pin_docs.py` fails CI if this table, [WIRING.md](WIRING.md) and pins.h disagree.

## The board

- **Module:** ESP32-WROOM-32D (PCB antenna; usually 4 MB flash).
- **Board:** 30-pin "ESP32 DevKit V1" layout, USB-C with a USB-serial chip, AMS1117 3.3 V regulator, blue LED on GPIO2 (silkscreen "D2"), BOOT and EN buttons. Sold as "NodeMCU-32S ESP32-WROOM-32D 30Pin Type C". Matched to Shai's photos of the board and its bag label (17 Oct 2026).
- **Carrier:** MRD068A screw-terminal adapter. It only brings each header pin out to a terminal; it adds no parts to any GPIO.

Sources: [ESP32-WROOM-32D datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf) (pin definitions, strapping pins), [ESP32 datasheet](https://documentation.espressif.com/esp32_datasheet_en.pdf) (IO_MUX, ADC channels), [ESP32 errata: GPIO36/39 pulled down when the ADC powers up](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32/03-errata-description/esp32/gpio-inputs-pulled-down.html).

## Rules the build enforces

pins.h runs these as `static_assert`s, so breaking one stops every build, and `test/test_pins` names the pin that breaks it.

1. **No GPIO used twice.**
2. **Only header pins.** GPIO0 (BOOT button) and GPIO6–11 (wired to the flash chip) are not on the header. GPIO1/3 are the USB serial pair and stay free for flashing and the serial monitor.
3. **No outputs on GPIO34–39.** They are input only and have no internal pull-up or pull-down.
4. **Analog inputs on ADC1 only (GPIO32–39).** ADC2 can't be read while the radio is on.
5. **No pulse counting or interrupts on GPIO36/39.** Every ADC conversion pulls them LOW for about 80 ns, which would count as a wheel tick.
6. **Kill lines must be LOW from power-on.** Not on a pin that is HIGH or toggles during boot (1, 3, 5, 14, 15).
7. **Strapping pins carry only signals that match their boot level.** GPIO2 and GPIO12 must be LOW at reset, so they carry kill lines with pull-downs. GPIO5 and GPIO15 idle HIGH, so they carry a UART TX and a 1-Wire bus with pull-ups.
8. **At most 16 PWM outputs** (the LEDC has 16 channels). We use 10.
9. **I2C addresses and PCF8574 bits are unique.**

## Pin by pin

Header pins are counted from the top (antenna end), with the USB connector at the bottom: L1–L15 on the left, R1–R15 on the right.

| Header | GPIO | Signal | Dir | Connects to | Why this pin, and what to watch |
| --- | --- | --- | --- | --- | --- |
| L1 | — | EN | — | EN button | Resets the chip. Leave unconnected. |
| L2 | 36 | `PEDAL` | in, analog | Pedal signal through 10k series, 100k pull-down, 100 nF, 3.3 V clamp | ADC1 channel 0 ("VP"). Analog only: the ADC glitch on this pin is harmless for a voltage reading. Never more than 3.3 V. |
| L3 | 39 | `BATTERY_SENSE` | in, analog | 150k from battery +, 12k to GND, 100 nF | ADC1 channel 3 ("VN"). 2.2 V at 29.4 V, inside the accurate ADC range. |
| L4 | 34 | `WHEEL_SPEED_L` | in, pulse | Left A3144 hall output, 10k pull-up to 3.3 V, 100 nF | Input only, no internal pull-up, so the external one is required. To be counted with the PCNT unit. |
| L5 | 35 | `WHEEL_SPEED_R` | in, pulse | Right A3144 hall output, 10k pull-up to 3.3 V, 100 nF | Same as GPIO34. |
| L6 | 32 | `RL_RPWM` | out, PWM | Rear-left IBT-2 RPWM (forward) | Plain I/O. |
| L7 | 33 | `RL_LPWM` | out, PWM | Rear-left IBT-2 LPWM (reverse) | Plain I/O. |
| L8 | 25 | `FL_RPWM` | out, PWM | Front-left IBT-2 RPWM (forward) | Plain I/O. |
| L9 | 26 | `FL_LPWM` | out, PWM | Front-left IBT-2 LPWM (reverse) | Plain I/O. |
| L10 | 27 | `FR_RPWM` | out, PWM | Front-right IBT-2 RPWM (forward) | Plain I/O. |
| L11 | 14 | `FR_LPWM` | out, PWM | Front-right IBT-2 LPWM (reverse) | Toggles during boot. Harmless: DRIVE_EN holds the drivers off until the car is armed. |
| L12 | 12 | `CONTACTOR_HOLD` | out, kill line | Q1 MOSFET gate through 100 Ω, 10k pull-down | Strapping pin: HIGH at reset selects 1.8 V flash and the board won't boot. The pull-down that keeps the contactor open also keeps the strap LOW. Never add a pull-up. |
| L13 | 13 | `RR_RPWM` | out, PWM | Rear-right IBT-2 RPWM (forward) | Plain I/O. |
| L14 | — | GND | — | Star ground | |
| L15 | — | VIN | — | 5V_LOGIC rail (LM2596) | 5 V in, not 3.3 V. |
| R1 | 23 | `I2C_SDA` | I2C | All I2C devices, 4.7k pull-up to 3.3 V | The Arduino default SDA is GPIO21, which DRIVE_EN uses. Set with `Wire.begin(23, 22)`. |
| R2 | 22 | `I2C_SCL` | I2C | All I2C devices, 4.7k pull-up to 3.3 V | Arduino default SCL. |
| R3 | 1 | TX0 | — | USB serial | Kept free: flashing and logs. |
| R4 | 3 | RX0 | — | USB serial | Kept free: flashing and logs. |
| R5 | 21 | `DRIVE_EN` | out, kill line | R_EN + L_EN of all four drive IBT-2s, 10k pull-down | Plain I/O, quiet at boot. The most important pin on the board, so it gets a pin with no boot role at all. |
| R6 | 19 | `STEER_LPWM` | out, PWM | Steering IBT-2 LPWM (left) | Plain I/O. |
| R7 | 18 | `STEER_RPWM` | out, PWM | Steering IBT-2 RPWM (right) | Plain I/O. |
| R8 | 5 | `SOUND_TX` | out, UART | DFPlayer RX through 1k | Strapping pin that idles HIGH; a UART TX idles HIGH too. May output a short signal at boot, which the DFPlayer ignores. |
| R9 | 17 | `CRSF_TX` | out, UART2 | ELRS receiver RX | UART2's default TX. |
| R10 | 16 | `CRSF_RX` | in, UART2 | ELRS receiver TX | UART2's default RX. CRSF at 420000 baud. |
| R11 | 4 | `RR_LPWM` | out, PWM | Rear-right IBT-2 LPWM (reverse) | Plain I/O. |
| R12 | 2 | `STEER_EN` | out, kill line | R_EN + L_EN of the steering IBT-2, 10k pull-down | Strapping pin, must be LOW to flash. The on-board LED and the pull-down both hold it LOW, and the LED lights when steering is enabled. |
| R13 | 15 | `ONEWIRE_TEMP` | 1-Wire | DS18B20 box temperature, 4.7k pull-up to 3.3 V | Strapping pin that idles HIGH (boot log on); the 1-Wire pull-up agrees. Proposed by the draft schematic; was spare. |
| R14 | — | GND | — | Star ground | |
| R15 | — | 3V3 | — | Not connected | The board's 3.3 V regulator stays for the ESP32 alone; sensors use the separate 3V3_SENSE rail. |

## What moved in this audit

| Signal | Was | Now | Why |
| --- | --- | --- | --- |
| `PEDAL` | GPIO34 | GPIO36 | Make room for a wheel-speed input on 34 |
| `BATTERY_SENSE` | GPIO35 | GPIO39 | Make room for a wheel-speed input on 35 |
| `WHEEL_SPEED_L` | GPIO36 | GPIO34 | GPIO36/39 read a false LOW every time the ADC runs. With pedal and battery on the ADC, the wheel counters would gain phantom ticks hundreds of times a second |
| `WHEEL_SPEED_R` | GPIO39 | GPIO35 | Same |
| `SPARE` | GPIO15 | `ONEWIRE_TEMP`, GPIO15 | Matches the draft schematic's DS18B20 |

Everything else was checked and kept: no pin is used twice, and the kill lines already sit on the pins that suit them best.

## Pin budget

All 23 usable GPIOs are taken. Anything new goes on the I2C bus:

- **Switches and slow inputs:** PCF8574 P5–P7 are free.
- **Analog:** ADS1115 #2 A1–A3 are free.
- **PWM for lights and accessories:** PCA9685, 16 channels.
- **More I2C devices:** the BMP280 you have answers at 0x76 and fits without a clash.

Two needs don't fit today and are open:

1. **Per-wheel speed.** There are hall inputs for the left and right sides only. Four independent wheels (traction control, e-diff) would want four. Options: two more hall sensors on a small I2C counter, or rely on per-wheel current from the ADS1115 for slip and fault detection.
2. **The HC-12 radio** needs a UART and there is none left. ELRS replaces it, so it is not planned.

## Optional hardening

Burning the flash-voltage eFuse (`espefuse.py set_flash_voltage 3.3V`) makes the board ignore GPIO12 at boot, so a failed contactor gate could never stop the board from booting. It is permanent, so it is a choice for later, after the bench test.
