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

## Telemetry (later)

After the CRSF telemetry module is written, discover sensors in the model's telemetry page. Add voice alerts for battery voltage, link quality below 70 %, and mode changes.
