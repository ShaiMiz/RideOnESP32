# Safety rules

These rules come before every feature. If a rule and a feature disagree, the rule wins.

## Before a child rides

All of these must be true and written down in [docs/COMMISSIONING.md](docs/COMMISSIONING.md):

- [ ] Phases 1 to 6 have passed their gates, in order.
- [ ] The key switch and the E-stop cut motor power with the ESP32 unplugged.
- [ ] Turning the radio off stops the car within 0.5 s, measured, at the highest speed cap.
- [ ] A fault that clears on its own does not restart the motors while the pedal or stick is held.
- [ ] The battery chemistry is known and `include/config.h` battery thresholds match it.
- [ ] The current-sense resistor on the IBT-2 boards has been read and `IS_PIN_RESISTOR_OHMS` matches it (the firmware halves it, because R_IS and L_IS are joined).
- [ ] Logged current agrees with a meter.
- [ ] Stopping distance has been measured at the speed cap the child will use, with a sandbag of the child's weight.

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
