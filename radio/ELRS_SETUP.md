# ExpressLRS setup

The radio's built-in ELRS module and the Jumper AION RX Mini receiver.

1. **Same firmware.** Update both to the same ExpressLRS 3.x release with the ExpressLRS Configurator. A mismatch can fail to bind or behave oddly.
2. **Binding phrase.** Set a long, unique phrase in both. It stops your radio from binding to anyone else's receiver, and theirs to yours. ELRS has no encryption, so this prevents accidents, not a determined attacker.
3. **Model Match.** Turn it on, and set the receiver's model ID to the `RideOn` model. The car then ignores the radio when a different model is selected.
4. **Receiver Wi-Fi off.** Turn off the receiver's automatic Wi-Fi hotspot ("Wi-Fi auto on interval"). It starts by default after a period without a link.
5. **Packet rate.** 100 or 150 Hz. That is plenty for a car and more robust than 500 Hz.
6. **Power.** Dynamic power, maximum 100 mW. The goal is a clean link at 50 m, not kilometres.
7. **Telemetry ratio.** Default (or 1:8 or faster) so link quality and, later, battery telemetry come back.
8. **Failsafe.** No Pulses (the default).
9. **Receiver power.** Feed the AION RX Mini 5 V from the logic rail, not 3.3 V.
10. **Wiring.** Receiver TX to ESP32 GPIO16, receiver RX to ESP32 GPIO17, plus 5 V and GND. The link runs CRSF at 420000 baud.

Check it with the `radio_check` bench firmware (phase 2 in [docs/COMMISSIONING.md](../docs/COMMISSIONING.md)).

Do not flash ExpressLRS onto the car's ESP32. One archived PDF suggests it, but that would erase the car firmware.
