# Audio

Two separate paths, so music can never affect the drive electronics and the ESP32 never runs Bluetooth. Phase 7 work; nothing here is bought or wired yet.

| Path | Source | Amp and power | Speakers | Issue |
| --- | --- | --- | --- | --- |
| Music | Phone over Bluetooth to the amp board | ZK-502T Bluetooth amp on the 12 V accessory rail ([POWER_CHAIN.md](POWER_CHAIN.md)) | 2 × 3" full-range, plus Shai's DDT-S30 tweeters | [#47](https://github.com/ShaiMiz/RideOnESP32/issues/47) |
| Car sounds | ESP32 over serial (`SOUND_TX`) | DFPlayer Mini on the 5 V accessory rail | One small alert speaker | [#46](https://github.com/ShaiMiz/RideOnESP32/issues/46) |

## Notes

- **Tweeters:** the DDT-S30 only plays above about 4 kHz. Each one gets a 10 µF non-polarised film capacitor in series as its crossover, and runs next to a full-range speaker that covers mids and bass.
- **Bluetooth lives on the amp board only.** SAFETY.md still holds: the ESP32's Wi-Fi and Bluetooth stay off.
- **DFPlayer on 5V_ACC, not the logic rail**, so speaker current stays off the ESP32 supply (schematic sheet 4).
- **Loudness:** EN 71-1 sets limits for toys near a child's ear. The [safety-standards review](../reviews/2026-10-safety-standards.md) lists audio loudness as a gap: cap the amp's volume and measure it before a child rides with music on.
- **Mounting:** speaker spots are not chosen yet. They wait on photos of the dashboard.
