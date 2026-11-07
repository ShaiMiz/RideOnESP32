# Radio setup: Jumper T-Lite V2 and AION RX Mini

Almost everything is done by files and scripts. You plug in USB, copy, flash and press a few buttons; you never build the model by hand. The steps take about 45 minutes the first time, most of it waiting for downloads.

What does what:

| Part | Done by | What it sets |
| --- | --- | --- |
| `RideOn` model | [edgetx/sdcard/MODELS/model42.yml](edgetx/sdcard/MODELS/model42.yml), copied by `install_radio.py` | Channels, switches, trims off, start-up switch warnings, receiver number 42 for Model Match, telemetry sensors, telemetry screen, telemetry logging |
| Telemetry screen and alarms | [SCRIPTS/TELEMETRY/rideon.lua](edgetx/sdcard/SCRIPTS/TELEMETRY/rideon.lua) | Mode and fault, battery, current, link quality; vibrates and beeps on a new fault, mode change, low battery, weak link and lost link |
| ELRS module settings, Model Match, version check | [SCRIPTS/TOOLS/RideOnRF.lua](edgetx/sdcard/SCRIPTS/TOOLS/RideOnRF.lua) on the radio ("RideOn ELRS setup") | Packet rate, switch mode, telemetry ratio, power; teaches the receiver this model; writes the exact radio, module and receiver versions to the SD card |
| Binding phrase, receiver Wi-Fi off | ExpressLRS Configurator, when flashing | Compiled into both firmwares |
| Receiver version, binding-phrase and Wi-Fi check, firmware and settings backup | [receiver_check.py](receiver_check.py) (`receiver_check.cmd`), over the receiver's own Wi-Fi | Nothing: it only reads, and saves the backup |
| SD backup, install, version and stick-mode check, log archive | [install_radio.py](install_radio.py) (double-click `install_radio.cmd` on Windows) | Everything above that lives on the SD card |
| Checks | CI: `tools/check_radio_files.py`, `tools/test_radio_scripts.py` | Model matches the firmware's channel map and limits; both Lua scripts and the installer run against simulations |

Detail: [MODEL_SETUP.md](MODEL_SETUP.md) (the model, switch by switch), [ELRS_SETUP.md](ELRS_SETUP.md) (the link settings and why), [DEVICE_FIRMWARE.md](../docs/hardware/DEVICE_FIRMWARE.md) (flashing, recovery, version log).

## The hardware

| Device | What it is | EdgeTX / ExpressLRS name |
| --- | --- | --- |
| Radio | Jumper T-Lite V2, STM32F4, 128×64 screen, EdgeTX | EdgeTX target `tlitef4` ("Jumper T-Lite (F4 MCU)" in EdgeTX Buddy) |
| Radio's ELRS module | Internal 2.4 GHz ESP32 module | Configurator: Jumper 2.4 GHz → "Jumper AION T-Lite 2.4GHz TX" |
| Receiver | Jumper AION RX Mini, ESP8285 + SX1280, 2.4 GHz | Configurator: Jumper 2.4 GHz → "Jumper AION Mini 2.4GHz RX" |

The exact versions on your units are read in steps 1 and 7 and saved in `RideOn-radio.txt` on the SD card; `install_radio.py` prints them and keeps a dated copy on the PC. Put them in the version log in [DEVICE_FIRMWARE.md](../docs/hardware/DEVICE_FIRMWARE.md#version-log).

## Target versions (checked 7 Nov 2026)

| Software | Version | Why |
| --- | --- | --- |
| EdgeTX | **2.12.5** | Latest stable release. The model file follows the 2.12 format; the installer refuses to call anything older than 2.10 good |
| ExpressLRS (module and receiver) | **4.1.0** | Latest release. Both targets are still listed for 4.x in the ExpressLRS target list, and the setup tool's setting names were checked against 4.1.0 and 3.6.4. Module and receiver must run the same version: 3.x and 4.x do not link |
| ExpressLRS Configurator | Latest from [github.com/ExpressLRS/ExpressLRS-Configurator/releases](https://github.com/ExpressLRS/ExpressLRS-Configurator/releases) | Flashes with the binding phrase and options built in |
| EdgeTX Buddy | [buddy.edgetx.org](https://buddy.edgetx.org) in Chrome or Edge | Flashes EdgeTX and the SD card contents over USB |

If ELRS 4.1 gives any trouble, 3.6.4 is the fallback: flash both to it, the rest of this setup is the same.

## Before the radio has a battery: the receiver alone

The receiver can be checked and flashed with nothing but a bench supply. The ESP32 and the car come later.

1. Set the bench supply to **5.0 V** with the current limit at **0.2 A**, output off. Connect only the receiver's **5V and GND** (check polarity); leave TX and RX loose. Output on: the LED blinks slowly.
2. Wait 60 s. With no radio around, the receiver opens a Wi-Fi network called `ExpressLRS RX` (LED blinks fast). Join it from the PC, password `expresslrs`. The PC has no internet while it is joined.
3. Double-click `radio/receiver_check.cmd`. It prints the receiver's product, target and exact firmware version, whether its binding phrase is ours, and whether its automatic Wi-Fi is on. It also **backs the receiver up** to `Documents\RideOnESP32-radio\receiver-backup-<date>\`: the running firmware (`.bin`, with its settings built in, so it can be flashed straight back from the receiver's Update page or the Configurator) and its config, options and hardware files. Run it before flashing, so the factory firmware is kept. The first run also creates the binding phrase there; the installer reuses it later for the radio.
4. To flash it now, stay on the `ExpressLRS RX` network and do step 5 below with flashing method **WiFi**. The receiver reboots, and because auto Wi-Fi is now off it won't open the hotspot by itself again: that is expected.
5. Check it: power-cycle the receiver, wait 2 minutes, and confirm no `ExpressLRS RX` network appears. To read it again later, use the radio's ExpressLRS Lua → WiFi Connectivity → Enable RX WiFi once the radio is bound, then run `receiver_check.cmd`; the report should read "All good".

Why this can't be done for you remotely: the PC has to leave your home Wi-Fi to join the receiver's network, which cuts the PC off from the internet (and from Claude) until it rejoins.

## Steps

You need: the T-Lite with a charged 18650, a USB-C data cable, a Windows PC, and for steps 6 to 9 the receiver wired to the ESP32 (phase 2, [COMMISSIONING.md](../docs/COMMISSIONING.md)).

1. **Back up and identify.** Turn the radio on, plug in USB, choose **USB Storage (SD)**. Double-click `radio/install_radio.cmd`. It backs up the whole SD card to `Documents\RideOnESP32-radio\backups\`, installs the RideOn files, prints the EdgeTX version and stick mode, and creates your **binding phrase** (saved in `Documents\RideOnESP32-radio\binding-phrase.txt`, never in git). Eject and unplug.
2. **Update EdgeTX** if step 1 said it is older than 2.12.5: open EdgeTX Buddy, pick the latest release and "Jumper T-Lite (F4 MCU)", tick "update SD card contents", and follow it (radio off, plug USB in, it flashes in DFU mode). Then run `install_radio.cmd` again, because the new SD contents need the RideOn files put back.
3. **Check the radio.** On the radio: SYS → Hardware → Internal RF is **CRSF** (it is from the factory on the ELRS T-Lite), and Radio setup → Mode is **2**. Select the RideOn model if it isn't already. The radio complains about switches until SA, SB and SC are all **up**: that is the start-up check working.
4. **Flash the radio's ELRS module.** In the Configurator: Jumper 2.4 GHz → "Jumper AION T-Lite 2.4GHz TX", release 4.1.0, regulatory domain **ISM 2400 (FCC)**, paste the binding phrase, flashing method **EdgeTXPassthrough**. On the radio: SYS → Hardware → Serial port USB-VCP = **CLI**; plug USB, choose **USB Serial (VCP)**; click Flash. Details and the Wi-Fi alternative: [DEVICE_FIRMWARE.md](../docs/hardware/DEVICE_FIRMWARE.md#t-lite-internal-elrs-module-expresslrs-tx).
5. **Flash the receiver** (or do it early, on the bench supply, [above](#before-the-radio-has-a-battery-the-receiver-alone)). Configurator: "Jumper AION Mini 2.4GHz RX", the same release, domain and phrase, and **untick AUTO_WIFI_ON_INTERVAL** (this turns the receiver's automatic Wi-Fi hotspot off). Flash over Wi-Fi: power the receiver (ESP32 on USB powers it), wait 60 s for its `ExpressLRS RX` hotspot (the current firmware still has auto Wi-Fi on), join it with password `expresslrs` and flash, or use the radio's ExpressLRS Lua → WiFi Connectivity → Enable RX WiFi if it is already bound. Recovery over UART: [DEVICE_FIRMWARE.md](../docs/hardware/DEVICE_FIRMWARE.md#jumper-aion-rx-mini-receiver-expresslrs-rx).
6. **Bind.** Nothing to do: the same binding phrase in both binds them. Power both; the receiver LED goes solid.
7. **Run the setup tool twice.** Switches SA, SB, SC up (Locked, KILL, slowest).
   - Car **off** (receiver unpowered): SYS → Tools → **RideOn ELRS setup**. It sets packet rate 150 Hz, Hybrid switches, telemetry 1:8, 100 mW with dynamic power. Every line should end in `ok` or `set`; Model Match says "run again car ON".
   - Car **on**: run it again. It turns Model Match on, which teaches the receiver this model's number (42), and reads the receiver's version. Every line now ends in `ok`. It writes `RideOn-radio.txt` to the SD card both times.
8. **Collect the reports.** Run `receiver_check.cmd` once more if you like (Enable RX WiFi from the ExpressLRS Lua first). Then put the radio in USB Storage again and, run `install_radio.cmd`. It prints the report (exact names and versions of radio, module and receiver, and every setting) and warns about any version mismatch. Copy the versions into the [version log](../docs/hardware/DEVICE_FIRMWARE.md#version-log).
9. **Bench checks**, wheels off the ground, below.

## Checks

### In CI, every push

- `tools/check_radio_files.py`: every channel in `model42.yml` comes from the switch or stick [CHANNEL_MAP.md](../docs/firmware/CHANNEL_MAP.md) names, with the polarity the firmware expects (kill switch up = KILL); trims are off; the start-up switch warnings are on; receiver number 42; the alarm thresholds equal `BATTERY_LOW_WARN_V` and `LQ_DEGRADED_BELOW` in `config.h`; the file has no YAML comments (EdgeTX's reader has none).
- `tools/test_radio_scripts.py`: the telemetry script runs in Lua 5.2, as on the radio, against a mock EdgeTX (screen with and without link, fault and mode alerts, low battery repeat, weak and lost link). The setup tool runs against a simulated ELRS module that answers like the real firmware, including chunked replies and refusing a packet-rate change while connected; it must refuse to do anything unless SC is on KILL and SA on Locked. The installer and log summary run against a fake SD card.

What CI cannot check: that EdgeTX reads `model42.yml` exactly as intended (it was written against the EdgeTX 2.12.5 source, not loaded in a real radio), and the real radio link. Those are the bench and field checks.

### Bench, phase 2 (receiver on the ESP32, `radio_check` firmware)

- [ ] The RideOn model loads with no "unknown" items; Model → Mixes shows CH1 Steer, CH2 Thrtl, CH5 Kill, CH6 Mode, CH7 KidCap, CH8 Horn.
- [ ] Every stick and switch moves its channel as [CHANNEL_MAP.md](../docs/firmware/CHANNEL_MAP.md) says, in the right direction: SC up = CH5 below 1100, down = above 1900; SA up / middle / down = Locked / Kid / Parent; left stick sideways = horn.
- [ ] Letting go of the right stick reads `thr=+0.00 str=+0.00`.
- [ ] Turning the radio off prints `link OK -> LOST` with the last packet under 200 ms old.
- [ ] Model Match: select another model on the radio; the car reports link lost. Back to RideOn; link returns (and the car stays Locked until re-armed).
- [ ] Receiver Wi-Fi off: radio off, receiver powered, wait 2 minutes; no `ExpressLRS RX` network appears.
- [ ] The telemetry screen (long-press PAGE) shows `LOCKED`, the battery voltage and `LQ 100%`. Pressing the E-stop shows the fault inverted and blinking, with a buzz.

### Range and reliability, in the field

The radio logs telemetry twice a second to `LOGS/RideOn-<date>.csv` whenever the RideOn model is loaded. `install_radio.py` copies new logs to `Documents\RideOnESP32-radio\logs\` and prints, for each: link quality (min, 5th percentile, median), time below 70 % and 40 %, link drops and the longest one, the weakest signal and its SNR, transmit power, lowest battery and every mode or fault the car reported. `python radio/radio_logs.py <file>` does the same for one file.

The stock remote managed about 50 m. The ELRS target is a link that never drops anywhere a child should drive:

- [ ] **Range walk** (car on, Locked, wheels off the ground or the drivers unpowered): walk away along a street or field with the radio at chest height, to 150 m or the end of line of sight, and back. Then again with the radio held low and your body between radio and car. Pass: no drops, link quality never under 70 %.
- [ ] **Low-power margin**: in the ELRS Lua set Max Power to 10 mW, walk to 50 m. Pass: no drops. This shows a large margin at the 100 mW you drive with. Run "RideOn ELRS setup" afterwards to put 100 mW back.
- [ ] **Obstacles**: car behind a parked car, a wall, and inside a garage, with the radio 20 m away. Pass: no drops.
- [ ] **Driving**, phase 6, Parent mode, the full drive session logged. Pass: no drops, no unexpected faults in the summary.
- [ ] Repeat the range walk after any change to the receiver antenna position, the box or the firmware versions.

Keep each summary with the commissioning record. If a check fails, the summary shows whether it is signal (weak dBm: antenna placement, the receiver's antenna inside a metal box), interference (good dBm but low SNR or link quality) or power (TPWR not rising).
