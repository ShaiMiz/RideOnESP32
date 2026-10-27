# Jumper T-Lite V2 model setup

One EdgeTX model called `RideOn`. The channel numbers must match [docs/firmware/CHANNEL_MAP.md](../docs/firmware/CHANNEL_MAP.md) and `lib/rideon_core/src/ChannelMap.h`.

## Why the right stick

The T-Lite's left stick throttle axis does not spring back to centre. If throttle were on it, letting go would not stop the car. Both driving axes are on the **right stick**, which springs back on both axes, so letting go means stop and straight ahead.

## Mixes

| Channel | Source | Function | Notes |
| --- | --- | --- | --- |
| CH1 | Right stick horizontal (Ail) | Steering | Centre = straight |
| CH2 | Right stick vertical (Ele) | Throttle and brake | Centre = stop. Forward = drive. Back = reverse, or brake in Kid mode |
| CH3 | Left stick vertical (Thr) | Not used | Leave mixed; firmware ignores it |
| CH4 | Left stick horizontal (Rud) | Not used | |
| CH5 | 2-position switch (e.g. SA) | Kill / run | High = run. Low = kill. ELRS treats CH5 high as "armed" |
| CH6 | 3-position switch (e.g. SB) | Mode | Low = Locked, middle = Kid, high = Parent |
| CH7 | 3-position switch (e.g. SC) | Kid speed cap | Low = 30 %, middle = 60 %, high = 100 % |
| CH8 | Momentary switch or button | Horn | High = sound |

Pick switches you can find by feel. Put the kill switch where your thumb can reach it without looking.

## Outputs

- Every channel: range 988 to 2012 µs (EdgeTX -100 % to +100 %), centre 1500 µs.
- No expo on CH2 to start with. Add it later if the throttle feels jumpy near centre.
- Trims off on CH1 and CH2. A trimmed throttle that is not centred stops the car from arming.

## Failsafe

Set the receiver's failsafe to **No Pulses** (ELRS default). On link loss the receiver stops sending, the ESP32 sees no packets for 200 ms, and it brakes to a stop.

## Switch warnings

In the model's setup page, turn on start-up switch warnings for SA (run) and SB (Locked). The radio then complains at power-on if the car would not start in a safe state.

## Telemetry

The car sends these back through the receiver (CRSF telemetry, firmware `CrsfTelemetry`). The telemetry ratio in the ELRS settings must not be Off ([ELRS_SETUP.md](ELRS_SETUP.md)).

| EdgeTX sensor | What it is | Rate |
| --- | --- | --- |
| `RxBt` | Battery voltage at the box, 0.1 V | 4 per second |
| `Curr` | Drive current, the sum of the four wheel motors, 0.1 A | 4 per second |
| `Capa` | Charge used since power-on, mAh | 4 per second |
| `Bat%` | Always 0 until the battery chemistry is known (#13) | |
| `FM` | Mode and the first latched fault, e.g. `PARENT`, `LOCKED ESTOP`, `KILLED LINK+` (`+` = more than one fault) | Once a second, and at once on a change |
| `RQly`, `RSSI` | Link quality and signal, from the receiver itself | |

Fault names are the ones in [FAULT_CODES.md](../docs/firmware/FAULT_CODES.md). The serial console's `dtc` gives the details.

### Setup

1. Power the car and the radio. In the model's **Telemetry** page, choose **Discover new sensors**. `RxBt`, `Curr`, `Capa`, `Bat%` and `FM` appear within a few seconds. Stop discovery.
2. Put `RxBt`, `Curr` and `FM` on a telemetry screen, so the mode and fault are visible at a glance.
3. Alarms in **Logical switches** and **Special functions**:
   - **Low battery:** `L1: a<x RxBt <threshold>`, then `SF: L1 → Play value RxBt` every 10 s. The threshold waits on the battery chemistry (#13). Set it at `BATTERY_LOW_WARN_V` in `config.h` (22.0 V for now), so the radio warns together with the car and before the cutoff at `BATTERY_CUTOFF_V`.
   - **Weak link:** `L2: a<x RQly 70`, then a special function that plays a "signal low" sound. 70 % matches `LQ_DEGRADED_BELOW`, below which the firmware halves the speed cap.
   - **Telemetry lost:** EdgeTX warns by itself when the sensors stop. On this car that means the link is gone and the car is braking.
4. Voice for faults: EdgeTX logical switches can't compare text, so `FM` is shown, not spoken. A spoken fault alert comes with the Lua tools script ([edgetx/README.md](edgetx/README.md)).

### Check

With the car on the bench, wheels off the ground: `FM` reads `LOCKED`; switch to Parent and it reads `PARENT` within a second; press the E-stop and it reads `LOCKED ESTOP` (or `KILLED ...` with the kill switch) at once. `RxBt` matches a multimeter on the battery to within 0.3 V.
