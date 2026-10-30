# Safety rules

These rules come before every feature. If a rule and a feature disagree, the rule wins.

## Before a child rides

All of these must be true and written down in [docs/COMMISSIONING.md](docs/COMMISSIONING.md). The required list is what every shop-bought ride-on is lab-tested for (EN 71-1 / SI 562-1, EN 62115, ASTM F963); the recommended list goes beyond any factory. Why each item is here: [docs/reviews/2026-10-safety-standards.md](docs/reviews/2026-10-safety-standards.md).

### Required

- [ ] Phases 1 to 6 have passed their gates, in order.
- [ ] The key switch and the E-stop cut motor power with the ESP32 unplugged.
- [ ] Turning the radio off brings the motor output to zero within 0.75 s, measured, at the highest speed cap (200 ms to call the link lost, then 0.5 s of braking from full; Shai, 28 Oct 2026). The standard (EN 62115 Annex J) only asks that it stops by itself; the time is ours.
- [ ] A fault that clears on its own does not restart the motors while the pedal or stick is held.
- [ ] The battery chemistry is known and `include/config.h` battery thresholds match it.
- [ ] The key switch is rated for what it carries (#19), or it drives a relay.
- [ ] A fuse sits in each drive motor's + lead (F11-F14), sized from the motor label (#14).
- [ ] Locked-wheel test: with one wheel held and full throttle, its fuse or a fault cuts it, and no wire, motor, driver or box surface gets too hot to keep a hand on.
- [ ] Parked on a 10° slope with a sandbag of the child's weight and the key off, the car moves no more than 5 cm.
- [ ] Top speed at the highest cap is measured and within the limit for the rider's age: 16 km/h from age 6, 6 km/h under 6 (8.2 km/h only with a low/high switch that starts in low).
- [ ] No sharp edges, screw tips, cut metal or zip-tie ends where the child sits or reaches; fan guards on.

### Recommended

Do these when convenient. A failure here turns that feature off; it doesn't hold the project, as long as the required list passes.

- [ ] The current-sense resistor on the IBT-2 boards has been read and `IS_PIN_RESISTOR_OHMS` matches it (the firmware halves it, because R_IS and L_IS are joined). Required until F11-F14 are fitted.
- [ ] Logged current agrees with a meter. Required until F11-F14 are fitted.
- [ ] Stopping distance has been measured at the speed cap the child will use, with a sandbag of the child's weight.
- [ ] The contactor weld and motor-bus checks (`K1_WELD`, `NO_BUS`) behave as phase 3 describes.

## Every ride

- The parent holds the radio, switched on, for the whole ride, including in Kid mode.
- Start with the mode switch at Locked and the kill switch off.
- Start at the lowest speed cap.
- Flat, open ground away from roads, water and slopes.
- Helmet on.

## Never

- Never tie the BTS7960 enable pins to VCC. The enable line is a kill switch.
- Never judge the radio link by channel values. They freeze at their last value when the link drops.
- Never let a fault clear itself. Faults stay latched until the parent re-arms from Locked.
- Never remove the 10k pull-downs on GPIO21, GPIO2 and GPIO12.
- Never flash firmware with the car's wheels on the ground.
- Never turn on the ESP32's Wi-Fi or Bluetooth, or the receiver's Wi-Fi, while driving. The Bluetooth log module ([BLUETOOTH.md](docs/hardware/BLUETOOTH.md)) is a separate board that only listens: its TXD stays unconnected, so it can never send anything to the ESP32.
- Never change `config.h` limits without re-running the gate tests that depend on them.

## If something goes wrong

1. Hit the E-stop, or turn the key off.
2. Read the fault on the serial log (later also on the radio screen and the OLED). Each code is explained in [docs/firmware/FAULT_CODES.md](docs/firmware/FAULT_CODES.md).
3. Fix the cause before re-arming. A fault that keeps coming back is a hardware problem, not something to tune away.
