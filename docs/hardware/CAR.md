# The car

| Fact | Value | Source |
| --- | --- | --- |
| Model | Can-Am Maverick X3 24 V, 4WD | kolbosima.co.il item 8419715; confirmed by Shai |
| Drive motors | 4, one per wheel | Confirmed by Shai |
| Steering | Electric steering motor; the child steers by hand in Kid mode | |
| Weight / size | 42 kg, 147 × 86 × 88 cm, two seats | Listing |
| Rider limit | about 35 kg | Listing |
| Stock top speed | 4.5 / 6 / 7.5 km/h by gear | Listing |
| State | 100 % factory original; nothing wired yet | |

## Open checks

These change values in [include/config.h](../../include/config.h). Fill them in before phase 4.

- [ ] **Battery:** voltage, Ah, and lead-acid or lithium. A 24 V lead-acid pack is up to 28.8 V on the charger, and a 7-cell lithium pack reaches 29.4 V. The BTS7960 runs up to 27.5 V and locks out at 27.6 to 30 V. This sets `BATTERY_*` in config.h.
- [ ] **Drive motor label:** voltage and size. This sets `MOTOR_CURRENT_LIMIT_A`.
- [ ] **Steering motor label** and the stock controller inside the car.
- [ ] **IBT-2 current-sense resistor:** read it on one board up close. This sets `IS_PIN_RESISTOR_OHMS` (assumed 1 kΩ; the firmware halves it because R_IS and L_IS are joined).
- [ ] **Pedal:** potentiometer or on/off switch? This sets `PEDAL_*`.
- [ ] **The voltmeter module:** its label (probably an XH-M609 low-voltage cutoff), and the five bags whose labels came out blurred in the photos.
