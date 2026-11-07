# ExpressLRS setup

The radio's built-in ELRS module and the Jumper AION RX Mini receiver. The steps are in [README.md](README.md); this page says what each setting is and why. The **RideOn ELRS setup** tool on the radio (`SCRIPTS/TOOLS/RideOnRF.lua`) sets everything marked *tool*, reads it back, and reports it.

| Setting | Value | Set by | Why |
| --- | --- | --- | --- |
| Firmware | Same release on both, ExpressLRS **4.1.0** (fallback 3.6.4) | Configurator | 3.x and 4.x don't link; a mismatch within a major version can behave oddly |
| Regulatory domain | ISM 2400 (FCC), same on both | Configurator | LBT and non-LBT builds don't link with each other |
| Binding phrase | Long and unique, from `install_radio.py` | Configurator, both | Your radio binds only to your receiver and nobody else's. ELRS has no encryption: this prevents accidents, not a determined attacker |
| Receiver auto Wi-Fi | **Off** (untick `AUTO_WIFI_ON_INTERVAL`) | Configurator, receiver | Otherwise the receiver opens a Wi-Fi hotspot 60 s after it loses the radio, in a box next to a child |
| Packet rate | **150 Hz** (100 Hz Full if 150 Hz is missing) | *tool*, car off | Plenty for a car, and more robust than 500 Hz: longer range and better through bodies and walls |
| Switch mode | **Hybrid** | *tool*, car off | CH5 (kill) goes in every packet; CH6 to CH8 (mode, speed cap, horn) in turn, so at most about 50 ms behind |
| Telemetry ratio | **1:8** | *tool* | Room for the car's battery, current and fault reports plus link statistics |
| Max power | **100 mW** | *tool* | The 2.4 GHz limit in most countries, and far more than 150 m needs |
| Dynamic power | **Dyn** | *tool* | Sends with less power when the link is strong, more when it weakens |
| Model Match | **On**, receiver number 42 | *tool*, car on | The car ignores the radio when another model is selected |
| Failsafe | Nothing to set | | Over CRSF the receiver simply stops sending on link loss, and the ESP32 brakes within 0.75 s |
| Receiver power | 5 V from the logic rail, not 3.3 V | Wiring | |
| Wiring | Receiver TX to ESP32 GPIO16, receiver RX to GPIO17, plus 5 V and GND | Wiring | CRSF at 420000 baud, the receiver's default |

The tool won't do anything unless SC is on KILL and SA on Locked, and stops if either moves. ELRS itself refuses to change packet rate and switch mode while the receiver is connected, so the tool sets those with the car off and Model Match with the car on (the receiver learns the number only while connected). Run it once each way; every line then reads `ok`.

Model Match: when it is turned on while the receiver is connected, the module sends the model's receiver number (42, set in `model42.yml`) and the receiver stores it. From then on the receiver ignores any other model. If you ever re-flash the receiver, run the tool again with the car on.

Check it with the `radio_check` bench firmware (phase 2 in [docs/COMMISSIONING.md](../docs/COMMISSIONING.md)) and the checks in [README.md](README.md#checks).

Do not flash ExpressLRS onto the car's ESP32. One archived PDF suggests it, but that would erase the car firmware.
