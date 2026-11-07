# 3D radio view

An interactive Jumper T-Lite V2 with the RideOn model loaded, built with React Three Fiber like the [exploded view](../exploded-view/). Every mesh is made in code; there are no model or image files.

## What it reads

Nothing is copied: Vite bundles these straight from the repo (`?raw` imports in [`src/lib/repoData.js`](src/lib/repoData.js)), so the view always matches the commit it was built from.

| File | Gives |
| --- | --- |
| `radio/edgetx/sdcard/MODELS/model42.yml` | the model: mixes, logical switches, start-up switch check, trims, module |
| `lib/rideon_core/src/ChannelMap.h` | which channel the firmware reads for steering, throttle, kill, mode, Kid speed cap and horn |
| `include/config.h` | channel limits, deadband, switch thresholds, arming tolerance, speed caps, link and battery thresholds |

The T-Lite's controls come from EdgeTX's own hardware list for it (`radio/src/boards/hw_defs/tlite.json`): sticks LH, LV, RH, RV; switches SA and SB (3-position), SC and SD (2-position); trims T1 to T4; keys RTN, ENT, Up, Down, Left/SYS, Right/MDL. Switch sides follow [radio/MODEL_SETUP.md](../../radio/MODEL_SETUP.md). The shape is approximate, from photos, not a measured drawing.

## What it does

- **Drive it:** drag the stick tops, click the switches (Shift-click goes back) and the keys, or use the keyboard: arrows = right stick, A/D = left stick sideways, W/S = left stick up/down, 1-4 = SA SB SC SD, K = the kill switch to KILL. The right stick springs back both ways and the left stick sideways; the left stick up/down stays where it is left, as on the real radio. The Live tab has touch pads and switch buttons too.
- **Screen:** the radio's 128 x 64 screen, drawn live. SYS and MDL change page: the RideOn telemetry page (a port of `run()` in `rideon.lua`, with the switch line, car mode and fault, battery, current, charge, link quality, signal and power), a channel monitor, the mix list (active lines highlighted) and a controls page. Power-on runs the start-up switch check; ⏻ turns the radio off.
- **Outputs:** all 16 CRSF channels as EdgeTX sends them (−1024..1024 → CRSF ticks → the µs AlfredoCRSF hands the ESP32), and what the firmware makes of each (Run / KILL, Locked / Kid / Parent, 30 / 60 / 100 %, horn, throttle and steering %).
- **Car:** a picture of the firmware's rules (ChannelMap.h, ModeMachine, ArmingCheck, the mode and cap part of Controller): Killed, Locked until the mode switch is seen at Locked and the sticks are centred, Kid, Parent, link lost. Link quality and battery sliders drive the telemetry, the weak-link cap and the alarms; Sound plays `rideon.lua`'s beeps. It is not the firmware: no faults, the pedal released, no ramps.
- **Labels:** each switch and stick shows the channel it moves and, for a switch, what each position means, worked out by moving the control and watching the outputs, so they follow any edit.
- **Mapping editor:** for each channel, its mix lines (source, weight, offset, a logical-switch condition, add / multiply / replace, name); add, delete, invert, clear, or add a spare channel (CH9 to CH16). Logical switches L1 to L8 and the start-up switch check per switch. "Reset to the repo model" and "Clear all mixes".
- **Safety checks**, run on every edit over all 36 switch combinations and the stick extremes, judging what the channels do rather than how the mixes are spelled. Any of these blocks the export:
  - one switch position kills on its own whatever else is moved, it is a 2-position switch, and another position lets the car run; Kill comes from a switch, never a stick;
  - in the positions the start-up check asks for, Kill is KILL and Mode is Locked;
  - with the sticks let go (and the non-centring left stick anywhere), throttle and steering are inside the firmware deadband, so letting go stops the car; throttle is not on the left stick up/down in the chosen stick mode;
  - some switch position gives Locked, and Mode comes from a switch;
  - trims off, CRSF module, model name RideOn.
- **Export:** the edited model as `model42.yml` (inside a .zip when opened as a claude.ai Artifact, whose save has no .yml), Copy, and the diff against the repo model. Loading a .yml file or pasted text works too. An unedited model writes back byte for byte.

## Using an edited model

1. Save `model42.yml` and put it at `radio/edgetx/sdcard/MODELS/model42.yml` in your clone.
2. `python tools/check_radio_files.py` (needs `pip install pyyaml`). It checks the channel map, polarity and thresholds against the firmware. If you moved one of the car's functions to another control, also change `docs/firmware/CHANNEL_MAP.md`, `radio/MODEL_SETUP.md`, `EXPECTED` in `check_radio_files.py`, and `switchText()` in `rideon.lua` (the screen's top line reads SC, SA and SB itself). The Export tab lists what changed.
3. Radio in USB Storage mode, then `radio/install_radio.cmd`. It backs up the SD card first.
4. Commit on a branch and open a PR, so the radio and the repo agree.
5. Redo the phase 2 bench checks in [radio/README.md](../../radio/README.md), wheels off the ground.

## What it can't do

- It can't talk to the radio. The browser has no way into the T-Lite: the model goes over USB through `install_radio.py`.
- The keys (RTN, ENT, SYS, MDL, Up, Down, power) are fixed by EdgeTX and can't be remapped in a model.
- It edits mixes, logical switches and the start-up check only. The rest of the file (module, receiver number 42, telemetry sensors, screen, logging, trims, special functions) passes through untouched.
- Channel numbers are fixed by the firmware: moving a function to another channel needs `ChannelMap.h` to change too.
- ELRS Hybrid switch mode sends CH5 as on/off and CH6 to CH12 in a few steps; the view shows the values before ELRS.
- The mixer is a small subset of EdgeTX's (no curves, expo, delays, slow, inputs or flight modes). The model only uses that subset, and the editor offers no more. The real radio is still the check: the phase 2 bench list.

## Run it

```sh
cd tools/radio-view
npm ci
npm run dev            # http://localhost:5173
npm test               # model round-trip, channel map, safety checks, firmware rules
npm run build          # static site in dist/
npm run build:single   # one self-contained dist-single/index.html
```

## Code map

| File | What |
| --- | --- |
| `src/lib/edgetxYaml.js` | reads and writes EdgeTX's YAML subset, keeping every scalar's exact text |
| `src/lib/model.js` | model file ↔ editable config (mixes, logical switches, start-up check) |
| `src/lib/mixer.js` | sticks and switches → logical switches → mixes → channels → CRSF → µs |
| `src/lib/firmware.js` | ChannelMap.h decoding, ModeMachine and ArmingCheck, in JS |
| `src/lib/rules.js` | the safety checks and the diff against the repo model |
| `src/lib/uses.js` | which channels each control moves, and what a value means to the car |
| `src/lib/lcd.js` | the 128 x 64 screen pages |
| `src/components/Radio3D.jsx` | the radio |
| `src/components/LivePanel.jsx`, `MappingPanel.jsx`, `ExportPanel.jsx` | the side panel tabs |
