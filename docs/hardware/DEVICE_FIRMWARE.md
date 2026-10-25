# Device firmware and configuration

How to update, configure or change every device in the car. Only three devices run firmware you can update: the ESP32, the radio, and the two ELRS radios inside the T-Lite and the receiver. Everything else is set up with pins, buttons, a trim pot or files on a card.

Researched 17 Oct 2026. Sources are at the end.

## At a glance

| Device | Firmware | How it is updated or set up | Tool | Risk |
| --- | --- | --- | --- | --- |
| ESP32-WROOM-32D (car controller) | Our firmware, this repo | USB cable | PlatformIO | Low; can always be re-flashed over USB |
| Jumper T-Lite V2 radio | EdgeTX | SD card and bootloader, or EdgeTX Buddy over USB | EdgeTX Buddy, or a browser | Low with an SD backup |
| T-Lite internal ELRS module | ExpressLRS | Wi-Fi from the radio's ELRS Lua script, or EdgeTX passthrough over USB | ExpressLRS Configurator | Low |
| Jumper AION RX Mini receiver | ExpressLRS | Wi-Fi (started from the radio's Lua script), or UART with the boot button | ExpressLRS Configurator | Low; can be recovered over UART |
| AS5600 steering sensor | Settings only, some one-time | I2C from the ESP32 | Our firmware | **High: OTP burns are permanent and limited** |
| ADS1115, PCF8574, PCA9685, MPU-6050, SSD1306 OLED | None | I2C address pins; settings written by our firmware at boot | Our firmware | None |
| BTS7960 (IBT-2) drivers | None | Nothing to set. The IS resistor sets current scaling | Multimeter | None |
| LM2596 and other buck converters | None | Trim pot, set with a multimeter before connecting loads | Multimeter | Wrong voltage can kill the ESP32 |
| XH-M609 low-voltage cutoff (if used) | None | Front buttons | None | Low |
| DFPlayer Mini (sound) | None | Files on a microSD card | Any PC | None |
| HY2005RX stock controller (plan B) | None | Pairs with its stock 2.4 GHz remote | None | None |
| HC-12 433 MHz | AT commands | Not used in this project | None | None |

## Compatibility rules

- **Radio module and receiver must run the same ExpressLRS major version.** ELRS 4.x does not link with 3.x. Update both in the same session, transmitter first.
- **Stay on 3.x until 4.x is confirmed for both devices.** Upgrade only after checking that the Configurator lists both the "Jumper AION T-Lite 2.4GHz TX" and "Jumper AION Mini 2.4GHz RX" targets for that release. A working link is worth more than new features.
- **The radio needs EdgeTX 2.8.0 or newer** to talk to its internal ELRS module properly. EdgeTX 2.7 broke it.
- **Write down every version** you flash in the table at the bottom of this file.

## ESP32 (car controller)

The board is an ESP32-WROOM-32D with a USB-C port and a USB-to-serial chip (usually CP2102 or CH340).

**Setup on the PC (once):**
1. Install VS Code and the PlatformIO extension, or the PlatformIO CLI.
2. Plug in the board. If Windows shows no new COM port in Device Manager, install the CP210x or CH340 driver, depending on the chip printed on the board.

**Flashing:**
1. **Safety first:** turn the car's key off, so the contactor is open and the drivers have no power. The kill-line pull-downs keep the drivers off while the ESP32 resets, but don't rely on that while flashing.
2. `pio run -e <env> -t upload`, where `<env>` is `radio_check`, `one_motor` or `car`.
3. If the upload stalls at `Connecting....`, hold the board's BOOT button until it starts writing.
4. `pio device monitor` shows the serial log at 115200 baud. Check the boot line's reset reason.

**Recovery:** if the board misbehaves after a bad flash, erase it with `pio run -e car -t erase`, then upload again.

**Strapping pins:** GPIO12 must be LOW at boot, which the contactor pull-down guarantees. GPIO2 must be LOW or floating to enter download mode, which the steering-enable pull-down guarantees. Don't add pull-ups to either.

**No over-the-air updates.** Wi-Fi and Bluetooth stay off on the car (see [SAFETY.md](../../SAFETY.md)), so the firmware is flashed over USB only.

## Jumper T-Lite V2 radio (EdgeTX)

**Back up first:** connect USB, choose "USB Storage" on the radio, and copy the whole SD card to the PC.

**Easiest path: EdgeTX Buddy** (buddy.edgetx.org in Chrome or Edge). It downloads the firmware and SD card contents and flashes them over USB. To flash, the radio goes into DFU mode: with the radio **off**, plug in the USB cable. The T-Lite has no DFU button.

**Bootloader path (SD card):**
1. Download the EdgeTX release and copy the T-Lite `.bin` into the SD card's `FIRMWARE` folder.
2. Update the SD card contents from the `edgetx-sdcard` release for the 128×64 black-and-white screen (`bw128x64`).
3. To update the bootloader: power on normally, open the SD card browser, select the `.bin` and choose "Flash bootloader".
4. To write the firmware: with the radio off, push both inner horizontal trims toward the centre and press power. Choose "Write Firmware", pick the `.bin`, and long-press to flash. "Invalid firmware" means the file is for a different radio.

**Our configuration** lives in [radio/MODEL_SETUP.md](../../radio/MODEL_SETUP.md). After an EdgeTX update, check that the `RideOn` model and its switch warnings survived. Later, the EdgeTX Lua script in [radio/edgetx/](../../radio/edgetx/) is copied to `SCRIPTS/TOOLS/` on the SD card.

## T-Lite internal ELRS module (ExpressLRS TX)

**ExpressLRS Configurator settings:** Device Category "Jumper 2.4 GHz", Device "Jumper AION T-Lite 2.4GHz TX", the official release you chose, regulatory domain (mandatory: ISM 2.4 GHz, or CE LBT in the EU), and the **binding phrase**.

**Method 1: Wi-Fi (no cables)**
1. In the Configurator, click **Build** and keep the output folder open (`firmware.bin`, or `firmware.bin.gz` on 3.3+; don't unzip it).
2. On the radio, open SYS → Tools → ExpressLRS → WiFi Connectivity → Enable WiFi.
3. Join the `ExpressLRS TX` hotspot (password `expresslrs`) and open `10.0.0.1`.
4. On the Update tab, upload the file and click Update. Wait for the success message.

**Method 2: EdgeTX passthrough (USB)** is the method one reviewer found reliable on the T-Lite:
1. On the radio, set Hardware → Serial ports → USB-VCP to CLI.
2. Plug in USB and choose "USB Serial (VCP)" on the radio. Windows should show an STMicroelectronics Virtual COM Port; install the ST VCP driver if it doesn't.
3. In the Configurator, set the Flashing Method to **EdgeTXPassthrough** and click Flash.

**Check it:** reload the ExpressLRS Lua script and read the version line.

**Settings from the Lua script** (no reflash needed): packet rate (100 or 150 Hz), maximum and dynamic power (100 mW), telemetry ratio, and Model Match. Our values are in [radio/ELRS_SETUP.md](../../radio/ELRS_SETUP.md).

## Jumper AION RX Mini receiver (ExpressLRS RX)

**Configurator settings:** Device Category "Jumper 2.4 GHz", Device "Jumper AION Mini 2.4GHz RX", the **same release and binding phrase** as the radio.

**Method 1: Wi-Fi.** Use this one; the receiver stays wired to the ESP32.
1. Power the car's 5 V logic rail (key on, or the ESP32 on USB if it powers the receiver), with the radio on and bound.
2. On the radio: ExpressLRS Lua → WiFi Connectivity → **Enable RX WiFi**. The receiver LED blinks fast.
3. Join the `ExpressLRS RX` hotspot (password `expresslrs`) and open `10.0.0.1`.
4. Upload the Configurator build on the Update tab, or set the Configurator's Flashing Method to WiFi and click Flash.
5. The LED goes back to slow blinking when it's done. Rebind if needed and check the link with `radio_check`.

We turn the receiver's **automatic** Wi-Fi off, which otherwise starts after 60 s without a link. The Lua "Enable RX WiFi" route still works because it only needs the receiver bound to the radio. If the receiver is ever unbound and auto Wi-Fi is off, use Method 2.

**Method 2: UART (recovery).**
1. Unplug the receiver from the ESP32 and wire it to a USB-to-UART adapter: 5 V, GND, receiver TX to adapter RX, receiver RX to adapter TX.
2. Hold the receiver's boot button and plug in the adapter. Release it when the LED is solid.
3. In the Configurator, set the Flashing Method to UART and click Flash.

A **solid LED** with the radio off means bootloader mode, or a failed update. Re-run Method 2.

**Receiver settings from its web page** (while in Wi-Fi mode): Auto WiFi On Interval (off), Model Match ID, failsafe mode (No Pulses), serial protocol (CRSF). Our ESP32 reads CRSF at 420000 baud.

Betaflight passthrough does not apply here: the receiver is wired to our ESP32, not a flight controller.

## AS5600 steering angle sensor (phase 5)

The AS5600 holds its zero position, maximum position and configuration in registers that the ESP32 writes over I2C at every boot. **Use only those volatile writes.** It also has one-time-programmable (OTP) "burn" commands:

- ZPOS and MPOS (zero and end angle) can be burned **at most 3 times**, and only with a magnet detected.
- MANG and CONF (angle range and filter settings) can be burned **once**, and only before any angle burn.

**Do not burn the OTP.** Store the steering calibration in the ESP32's flash instead (calibration from the radio comes later), so the sensor can be re-mounted or swapped freely. Check the magnet's status register (MD, ML, MH bits) when mounting: the magnet sits 0.5 to 3 mm above the chip, centred.

## Modules with no firmware

| Module | What can be set | How |
| --- | --- | --- |
| ADS1115 (×2) | I2C address 0x48 to 0x4B | ADDR pin to GND (0x48), VDD (0x49), SDA or SCL. Gain and data rate are set by our firmware at boot |
| PCF8574 | Address 0x20 to 0x27 (0x38 to 0x3F on the PCF8574A) | A0 to A2 jumpers. Check which chip you have |
| PCA9685 | Address 0x40 to 0x7F; PWM frequency | A0 to A5 solder pads; frequency by our firmware |
| MPU-6050 | Address 0x68 or 0x69; range and filter | AD0 pin; the rest by firmware |
| SSD1306 OLED | Address 0x3C or 0x3D | Resistor on the back of the module |
| BTS7960 (IBT-2) | Nothing | Read its IS resistor value for `IS_PIN_RESISTOR_OHMS` in [include/config.h](../../include/config.h) |
| LM2596 bucks | Output voltage | Turn the trim pot with a meter on the output and no load connected. Set 5.0 V **before** connecting the ESP32 |
| XH-M609 | Cut-off and reconnect voltage | Double-press the left button for the cut-off voltage. Long-press the right button for the reconnect offset. Short press = 0.1 V, long press = 1 V. Sellers' instructions vary, so check with a bench supply. The relay is rated 20 A (run it at 10 A or less), so it can't carry motor current. It could cut the accessory feed |
| DFPlayer Mini | Sounds | microSD up to 32 GB, FAT32. Put files in a folder named `MP3` as `0001.mp3`, `0002.mp3`, and so on. 9600 baud serial; add a 1 kΩ resistor in its RX line to avoid hum |
| HY2005RX stock controller | Nothing | Kept in its box as plan B. Pair it with its own remote if it is ever reinstalled |

## Version log

Fill this in every time something is flashed.

| Date | Device | From | To | By | Notes |
| --- | --- | --- | --- | --- | --- |
| | T-Lite EdgeTX | | | | |
| | T-Lite ELRS TX | | | | |
| | AION RX Mini | | | | |
| | ESP32 | | | | git commit: |

## Sources

- [ExpressLRS: Jumper internal TX modules](https://www.expresslrs.org/quick-start/transmitters/jumper-internal/)
- [ExpressLRS: Jumper AION receivers](https://www.expresslrs.org/quick-start/receivers/jumper-aion/)
- [ExpressLRS: updating receivers](https://expresslrs.org/quick-start/receivers/updating/)
- [Oscar Liang: Jumper T-Lite V2 review](https://oscarliang.com/jumper-t-lite-v2)
- [Oscar Liang: ExpressLRS 4.0 compatibility](https://oscarliang.com/expresslrs-4/)
- [EdgeTX: updating with the bootloader](https://manual.edgetx.org/installing-and-updating-edgetx/update-from-an-earlier-version-of-edgetx-using-the-bootloader)
- [EdgeTX: DFU and bootloader mode per radio](https://manual.edgetx.org/edgetx-how-to/access-dfu-and-bootloader-mode)
- [AS5600 OTP burn limits (CircuitPython driver docs)](https://circuitpython-as5600.readthedocs.io/en/stable/api.html)
- [XH-M609 settings](https://sigmanortec.ro/en/xh-m609-module-low-voltage-disconnect-7-50v)
- [DFPlayer Mini guide](https://wolles-elektronikkiste.de/en/arduino-controlled-dfplayer-mini)
