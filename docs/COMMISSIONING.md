# Commissioning

Each phase ends with a gate that must pass before the next phase starts. **No child rides until phase 6 passes.** Write the date and your initials when a gate passes.

Build the new electronics as a removable box and leave the stock wiring intact. The car can then go back to stock with the spare HY2005RX controller at any time.

## Phase 1: inventory and stock check

- [ ] Count the BTS7960 boards and confirm the four drive motors.
- [ ] Read the battery label: voltage, Ah, chemistry. Write it in [hardware/CAR.md](hardware/CAR.md).
- [ ] Read one drive motor label and the steering motor label.
- [ ] Drive the car once on its stock controller, to know its normal speed and feel.

**Gate:** motor count, battery chemistry and Ah written down. Passed: ____

## Phase 2: radio on the desk

- [ ] Charge the T-Lite (18650 cell).
- [ ] Flash the radio and receiver to the same ELRS 3.x and set the binding phrase and Model Match ([radio/ELRS_SETUP.md](../radio/ELRS_SETUP.md)).
- [ ] Build the `RideOn` model ([radio/MODEL_SETUP.md](../radio/MODEL_SETUP.md)).
- [ ] Wire only the receiver to the ESP32. Flash `pio run -e radio_check -t upload` and open the monitor.

**Gate:**
- [ ] Turning the radio off prints `link OK -> LOST` with the last packet under 200 ms old.
- [ ] Every stick and switch moves the channel [CHANNEL_MAP.md](firmware/CHANNEL_MAP.md) says it does, in the right direction.
- [ ] Letting go of the right stick reads `thr=+0.00 str=+0.00`.

Passed: ____

## Phase 3: power chain on the bench

- [ ] Fuse, key, E-stop, contactor, MOSFET and the 5 V buck on a board, with a lamp or one motor as the load ([hardware/POWER_CHAIN.md](hardware/POWER_CHAIN.md)).

**Gate:**
- [ ] With the ESP32 unplugged, the key and the E-stop each cut the load.
- [ ] With the ESP32 running and the contactor held, pressing the ESP32's reset button opens the contactor.

Passed: ____

## Phase 4: one motor, one driver

- [ ] One IBT-2 on the front-left pins, one motor, one ADS1115 at 0x48. Fill in `IS_RESISTOR_OHMS` and the battery thresholds in `config.h`.
- [ ] Flash `pio run -e one_motor -t upload`.

**Gate:**
- [ ] Pulling the receiver plug while the motor runs stops it.
- [ ] Turning the radio off while the motor runs stops it within 0.5 s.
- [ ] After any fault, holding the throttle never restarts the motor. It restarts only after the mode switch goes through Locked with the stick centred.
- [ ] The kill switch stops the motor and opens the contactor.
- [ ] The logged current agrees with a clamp meter at three loads (free spin, light brake, heavy brake).
- [ ] Driver temperature after 10 minutes at 16 kHz PWM is acceptable to the touch.

Passed: ____

## Phase 5: whole car, wheels off the ground

- [ ] Car on blocks. All five drivers, both ADS1115s, the PCF8574 and the pedal connected. Steering calibrated (once `SteeringController` is written).
- [ ] Flash `pio run -e car -t upload`.

**Gate:** every fault you can cause stops all wheels:
- [ ] Link loss (radio off, receiver unplugged)
- [ ] Low voltage (bench supply turned down), and the battery sense wire unplugged
- [ ] Kill switch, E-stop, key
- [ ] Pedal held while re-arming does not start the wheels
- [ ] Steering never fights a hand in Kid mode
- [ ] The parent stick-back brake overrides the pedal in Kid mode

Passed: ____

## Phase 6: ground tests, adult only

- [ ] An empty car, then a sandbag of the child's weight, at the lowest speed cap, in an open flat area.

**Gate:**
- [ ] Stopping distance measured at each speed cap: ____ / ____ / ____
- [ ] Link-loss stop measured at the top cap: ____
- [ ] Both acceptable.

Passed: ____

## Phase 7: kid rides, low cap

- [ ] Parent mode first, then Kid mode with the parent holding the radio.
- [ ] Add features from the list one at a time, each with its own bench test.
