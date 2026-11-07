# Jumper T-Lite V2 model: RideOn

You don't build this model by hand. It is the file [edgetx/sdcard/MODELS/model42.yml](edgetx/sdcard/MODELS/model42.yml), installed by `install_radio.py` ([README.md](README.md)). This page says what is in it and why, and how to check it on the radio. If you change the model on the radio, copy the change back into the file, or the next install overwrites it (the installer backs the card up first).

The channel numbers must match [docs/firmware/CHANNEL_MAP.md](../docs/firmware/CHANNEL_MAP.md) and `lib/rideon_core/src/ChannelMap.h`. CI checks the file against both (`tools/check_radio_files.py`).

## Why the right stick

The T-Lite's left stick throttle axis does not spring back to centre. If throttle were on it, letting go would not stop the car. Both driving axes are on the **right stick**, which springs back on both axes, so letting go means stop and straight ahead. The radio must be in **mode 2** (SYS → Radio setup → Mode); the installer warns if it isn't.

## Switches

The T-Lite has four toggle switches. They stick straight out of its back edge (the antenna side), two per side: SA (3-position) above SC (2-position) on the left, and SB (3-position) above SD (2-position) on the right. Up means the lever tip up, towards the sky as you hold the radio. The phase 2 channel check in [README.md](README.md) confirms it on the real radio.

| Channel | Control | Up | Middle | Down |
| --- | --- | --- | --- | --- |
| CH5 kill | **SC**, left 2-position | **KILL** | | Run |
| CH6 mode | **SA**, left 3-position | **Locked** | Kid | Parent |
| CH7 Kid speed cap | **SB**, right 3-position | **30 %** | 60 % | 100 % |
| CH8 horn | Left stick pushed sideways past half way, either way | | | |

Your right thumb drives; your left hand holds the radio with a finger on SC, so the kill switch is always under a finger. SC and SA feel different (2 and 3 positions), so they can be told apart without looking. SD is spare.

## What the file sets

- **Mixes:** CH1 = right stick sideways (`Ail`), CH2 = right stick up/down (`Ele`), CH3 and CH4 = the left stick (unused by the firmware), CH5 = SC, CH6 = SA, CH7 = SB, all at 100 %. CH8 = −100 % unless logical switch L1 (left stick sideways, |Rud| > 50 %) replaces it with +100 %. Range 988 to 2012 µs, centre 1500 µs, no expo.
- **Trims off:** all four trims are disabled, and CH1 and CH2 don't take trims. A trimmed throttle that is not centred stops the car from arming.
- **Start-up check:** at power-on the radio refuses to start sending until SA, SB and SC are all up (Locked, slowest, KILL). The throttle warning is off, because the left stick it watches is unused.
- **Internal module:** CRSF (ELRS), receiver number **42** for Model Match.
- **Failsafe:** none set in the radio. ELRS sends no failsafe positions over CRSF: when the link drops, the receiver stops sending, the ESP32 calls the link lost after 200 ms and brakes to a stop within 0.75 s ([SAFETY.md](../SAFETY.md)). Nothing on the radio can make that slower.
- **Telemetry sensors** made in advance, so no "Discover new sensors" is needed: `1RSS`, `RQly`, `RSNR`, `TPWR`, `TQly` from ELRS, and `RxBt`, `Curr`, `Capa`, `Bat%`, `FM` from the car.
- **Telemetry screen 1:** the `rideon` script.
- **Logging:** special function SF1 logs telemetry every 0.5 s to `LOGS/RideOn-<date>.csv` for the range checks.

## Telemetry

The car sends these back through the receiver (CRSF telemetry, firmware `CrsfTelemetry`). The telemetry ratio must not be Off ([ELRS_SETUP.md](ELRS_SETUP.md)).

| EdgeTX sensor | What it is | Rate |
| --- | --- | --- |
| `RxBt` | Battery voltage at the box, 0.1 V | 4 per second |
| `Curr` | Drive current, the sum of the four wheel motors, 0.1 A | 4 per second |
| `Capa` | Charge used since power-on, mAh | 4 per second |
| `Bat%` | Always 0 until the battery chemistry is known (#13) | |
| `FM` | Mode and the first latched fault, e.g. `PARENT`, `LOCKED ESTOP`, `KILLED LINK+` (`+` = more than one fault) | Once a second, and at once on a change |
| `RQly`, `1RSS`, `RSNR`, `TPWR` | Link quality, signal, signal-to-noise and the radio's transmit power, from ELRS itself | |

Fault names are the ones in [FAULT_CODES.md](../docs/firmware/FAULT_CODES.md). The serial console's `dtc` gives the details.

## Screen and alarms (`rideon.lua`)

Long-press PAGE on the main screen to see it. The top line shows what the radio's switches ask for (`KILL Lock 30%`); below it is what the car reports: its mode in large letters, a fault inverted and blinking, then battery, current, charge, link quality, signal and transmit power. With no telemetry it shows `NO LINK`.

The alarms run whenever the model is loaded, whichever screen is showing:

| Event | Alarm |
| --- | --- |
| New fault in `FM` | Two long vibrations and a falling tone |
| Car mode changes | 1 beep Locked, 2 Kid, 3 Parent; a low tone and a vibration for Killed |
| `RxBt` below 22.0 V (`BATTERY_LOW_WARN_V`) | A beep and the voltage spoken, every 10 s. The threshold waits on the battery chemistry (#13) |
| `RQly` below 70 % (`LQ_DEGRADED_BELOW`, where the car halves its speed cap) | Two high beeps and a short vibration, every 5 s |
| Telemetry lost | A long vibration, plus EdgeTX's own "telemetry lost". On this car that means it is braking to a stop |

The thresholds are copied from `include/config.h`; CI fails if they drift apart.

## Check on the radio

- Model → Mixes lists CH1 Steer, CH2 Thrtl, CH5 Kill, CH6 Mode, CH7 KidCap, CH8 Horn (twice). Channel monitor: SC up shows CH5 at −100 %, SA up CH6 at −100 %, the left stick sideways CH8 at +100 %.
- With the car on the bench, wheels off the ground: `FM` reads `LOCKED`; switch to Parent and it reads `PARENT` within a second, with 3 beeps; press the E-stop and it reads `LOCKED ESTOP` at once, with the fault alarm. `RxBt` matches a multimeter on the battery to within 0.3 V.
