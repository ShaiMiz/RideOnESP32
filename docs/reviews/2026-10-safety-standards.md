# Ride-on car safety: what the law requires vs what we gate on

Version 2, 25 Oct 2026 (v1: 17 Oct). Car: Can-Am Maverick X3 24 V 4WD (kolbosima item 8419715), stock controller HY2005RX-24V/40A. Our design as of main `0f59bd9`: SAFETY.md, COMMISSIONING.md, CONTROL_MODES.md, FAULT_CODES.md, `include/config.h`, KiCad schematic v0.3, radio/ELRS_SETUP.md, issues #1–#47.

**Marks:** 🔴 open mandatory item · 🟢 done or already better than the factory · 🔵 same as stock (we don't touch it) · ⚪ optional.
**Confidence:** ✔ checked in a source listed at the bottom. ◐ general knowledge of the standard, not checked against the paid text.

## Short answer

1. **The bar.** Every ride-on sold in Israel, the EU or the US is type-tested once per model by a lab against **EN 71-1 / SI 562-1** (mechanics: speed, brakes, stability), **EN 62115** (electrics: stalled motor, short circuit, temperatures, remote control) and, for the US, **ASTM F963 §4.25** (battery-operated toys, including a stalled-motor fire test). Two real lab reports for Chinese ride-ons (below) show exactly which items get ticked. Nobody measures stopping distance, link-loss time, current accuracy or EMC on a ride-on, and nobody fits an E-stop or contactor.
2. **What's mandatory for us.** 22 items. 11 are already met (🟢 or 🔵). **10 are open (🔴)**: speed in km/h, roll-away, a stalled motor with no backstop, the locked-wheel heat test, surface temperatures, the box's fire rating, switch ratings, sharp edges on our added parts, loudness, and nuisance tripping. The last one (M10) is half met: same battery, but its label (#13) is still unread, so the battery thresholds are a guess.
3. **What we can stop gating on.** Of our current "before a child rides" gates, 4 go beyond any standard: current calibration against a meter, stopping distance, the contactor weld and bus checks, and the 0.5 s link-loss time. They become **non-gating** (do them when convenient; a failure disables that feature rather than holding the project), provided we add the per-motor fuses in fix C.
4. **What to do first.** Read the battery label and key switch rating, set the km/h caps, add four per-motor fuses, then run the roll-away test and the locked-wheel test. That's all low-cost and covers every high-concern gap. The ranked list is at the end.

**Legal note ◐:** these laws bind whoever *sells* a toy. A parent rebuilding their own child's car isn't legally bound, and once the stock controller is out, the car's CE and SII approval no longer covers it. "Mandatory" below means "what every car on the shelf had to pass, so the bar we won't go below".

## Which laws and standards apply

| Market | Law | Standards a lab tests a 24 V ride-on to | Speed limit? | Remote-control rules? |
| --- | --- | --- | --- | --- |
| **Israel** | Standards Law; import needs SII approval ✔ | **SI 562 part 1** (mechanical, official since Nov 2022) ✔ + other SI 562 parts (flammability, chemicals) ✔. Electrics: EN/IEC 62115 report accepted ◐ | Yes, via SI 562-1 (built on EN 71-1) ◐ | Via EN 62115 Annex J ◐ |
| **EU** | Toy Safety Directive 2009/48/EC; the new **Toy Safety Regulation (EU) 2025/2509** applies from 1 Aug 2030 (adds a digital product passport) ✔ | **EN 71-1** ✔, EN 71-2/-3, **EN IEC 62115:2020+A11** ✔; radio: RED (EN 300 328, 100 mW EIRP at 2.4 GHz) ◐ | **Yes**: 3–6 y ≤ 6 km/h, or ≤ 8.2 km/h with a two-position switch shipped in low; 6 y+ ≤ 16 km/h ✔ | **Yes**: Annex J. Stops by itself on leaving range; a mode without the remote needs an adult and a tool ✔ |
| **US** | CPSIA (third-party test + Children's Product Certificate) ✔ | **ASTM F963-23** incl. §4.25 battery-operated toys, §8.17 stalled motor, §8.18 battery-powered ride-ons, §8.19 secondary batteries ✔ | **No**. CPSC asked ASTM in 2022 to consider one ✔ | No |
| **China** (domestic) | CCC ✔ | GB 6675 + **GB 19865** (= IEC 62115) ✔ | As ISO 8124-1 ◐ | ◐ |

## What factories really test

Two published lab reports for Chinese battery ride-ons ✔ (Intertek 2021, a 12 V lead-acid ride-on; and a 2024 report for a ride-on car, both to ASTM F963):

- **Ticked Pass:** flammability, sharp edges and points, projections, wheels and axles, stability and overload, sound-producing toys, max DC voltage, battery surface temperature, surface temperature after normal use and abuse, insulation temperature, **no fire risk with the motor stalled**, nuisance tripping of the fuse, switch ratings, battery and charger certification, **short-circuit protection of the main and motor wiring**, strain relief, labels and instructions, plus lead and phthalates.
- **Not in either report:** stopping distance, top-speed figures (US has no limit), remote-control behaviour, EMC immunity, current accuracy, anything about an E-stop or contactor. Neither report records a measured value for the electrical tests, only Pass.
- **Per unit on the line ◐:** a run test (forward, reverse, speeds, remote pairing) and visual checks. The deep tests happen once per model.

So the factory's whole electrical safety case is: **24 V, a fuse, rated wiring and switches, a stalled motor doesn't start a fire, and nothing a child touches gets too hot.** That is the bar.

## Mandatory items

"Factory tests it" means it appears as a tested clause in the standard or in the lab reports above.

| # | Item | Required by | Factory tests it | Our status | Fix: how | Complexity | Concern |
| --- | --- | --- | --- | --- | --- | --- | --- |
| M1 | **Top speed** ≤ 6 km/h (or ≤ 8.2 km/h with a low/high switch shipped in low) for ages 3–6; ≤ 16 km/h for 6 y+ | EN 71-1 4.15.1.8, EN 62115 13.10 ✔, SI 562-1 ◐ | EU/IL: yes. US: no | 🔴 Caps are % of motor power; Kid high and Parent are 100 % | **Code + test:** measure km/h at each cap in phase 6 (phone GPS or 10 m and a stopwatch), then set `KID_CAP_*` and `PARENT_CAP` in config.h. The speed applies in Parent mode too | Low | High |
| M2 | **Roll-away:** a ride-on of 30 kg or more must hold on the brake test ramp (≤ 5 cm), or creep ≤ 0.36 m/s with motor braking | EN 71-1 4.15.1.5, test 8.26.1 ✔ | EU/IL: yes | 🔴 Locked drops drive enable after stopping; kill, E-stop and key-off coast. Untested | **Test first:** car (and later + 35 kg sandbag) on a 10° slope, everything off. If the gearbox holds, done. If not: **code** keeps the low-side brake on while parked in Locked (contactor closed), and **parts**: 4 × 24 V 40 A SPDT automotive relays that short each motor when the contactor is open (the classic Power Wheels brake) | Low test; Medium fix | High |
| M3 | **Stalled motor:** a locked wheel must not overheat wiring or start a fire | EN 62115 cl. 9 ✔, ASTM 4.25.10.2 / 8.17 ✔ | Yes, every report | 🔴 One stalled motor likely draws < 20 A: software doesn't trip (20 A limit) and the 40 A main fuse never blows | **Parts:** a 15–20 A blade fuse in each motor lead (4 inline holders; size from the motor label, #14). **Code (optional):** an I²t heat-over-time limit per motor, with #44 | Low | High |
| M4 | **Short circuit** in main or motor wiring is cleared by a fuse | ASTM 4.25.10.7 ✔, EN 62115 cl. 9 ✔ | Yes | 🟢 40 A fuse within 15 cm; a fuse per rail. M3's motor fuses also protect the 12–14 AWG motor leads | Part of M3 | — | — |
| M5 | **Locked-wheel and heating test** as the lab runs it: hold full throttle on a locked wheel, record temperatures | EN 62115 cl. 9 ✔, ASTM 8.17 ✔ | Yes | 🔴 Not in COMMISSIONING.md | **Test:** add to phase 5: lock one wheel, full throttle 30 s, then until a fuse or fault trips; touch-check motor, driver, wiring and box | Low | High |
| M6 | **Accessible surfaces** stay under the temperature limits after normal use and abuse | EN 62115 Table 1 ✔, ASTM 4.25.7–4.25.8 ✔ | Yes | 🔴 Box and heatsinks not measured. `HOT` cut-out at 65 °C inside the box exists (PR #52) | **Test:** after 20 min of hard driving, and after M5, a hand on the box and any surface a child can reach is comfortable (about ≤ 50 °C; an IR thermometer helps) | Low | Medium |
| M7 | **Fire:** insulating parts that hold live connections resist heat and flame | EN 62115 cl. 18 ✔ (glow-wire ◐), ASTM switch rating ✔ | Yes | 🔴 Plain ABS box | **Parts:** a UL94 V-0 (flame-retardant) box, or a metal plate under the drivers, contactor and fuse block | Low | Medium |
| M8 | **Switches rated** for the current they switch | ASTM 4.25.10.4 ✔, EN 62115 ◐ | Yes | 🔴 Key switch rating unknown (#19) | **Read the rating.** If under about 10 A, the key drives a 30 A relay instead | Low | Medium |
| M9 | **Max 24 V nominal** | EN 62115 13.1 ✔, ASTM 4.25.2 ✔ | Yes | 🔵 Same battery | — | — | — |
| M10 | **Battery and charger** certified; no leaking in any position; cells protected | EN 62115 13.4, 15.5 ✔, ASTM 4.25.10.6, 4.25.11 ✔ | Yes | 🔵 Same pack and charger. 🔴 Chemistry unknown (#13), so the cut-off thresholds are a guess | **Read the label.** If lithium, keep its BMS. Charge port stays wired straight to the battery | Low | High if lithium |
| M11 | **Remote control (Annex J):** stops by itself when out of range | EN 62115 Annex J ✔ | EU/IL | 🟢 No packet for 200 ms or LQ < 40 % brakes to a stop; tested in phases 2 and 4 | Done. Only "it stops" is mandatory; the 0.5 s figure is ours | — | — |
| M12 | **Remote control (Annex J):** a mode without the remote needs an adult and a tool | EN 62115 Annex J ✔ | EU/IL | 🟢 No mode drives without the radio | Done | — | — |
| M13 | **Radio power** ≤ 100 mW EIRP at 2.4 GHz | RED / EN 300 328, Israel MoC ◐ | Radio type test | 🟢 ELRS_SETUP.md: dynamic power, max 100 mW (v1 of this report wrongly listed it open) | Done | — | — |
| M14 | **Electronic protection fails safe:** if a protection relies on electronics, a fault in that electronics must still leave the toy safe | EN 62115 Annex D ✔ (title), detail ◐ | Yes | 🟢 Watchdog, pull-downs, key and E-stop in hardware; a crash opens the contactor (phase 3 gate) | Done once phase 3 passes | — | — |
| M15 | **Stability** on the test slope with a child-mass load | EN 71-1 4.15.1.4 ✔, ASTM 4.15 ✔ | Yes | 🔵 Unchanged (our box is low under the seat) | — | — | — |
| M16 | **Strength** of seat and frame | EN 71-1 4.15.1.3 ✔ | Yes | 🔵 Unchanged | — | — | — |
| M17 | **Sharp edges, points, projections, pinch points** | EN 71-1, ASTM 4.7–4.9, 4.18 ✔ | Yes, every report | 🔴 Not checked on our added parts (box, E-stop mount, brackets, steering motor) | **Checklist** in phase 5: no exposed screw tips, cut metal or zip-tie ends where a child sits or reaches; fan guards on | Low | Medium |
| M18 | **Loudness** of sound-producing toys | ASTM 4.5 ✔, EN 71-1 4.20 (about 85 dB(A)) ◐ | Yes | 🔴 2 × 50 W amp and DFPlayer planned (#46, #47); BOM line says "cap the volume" | **Code:** cap the DFPlayer volume. **Parts/setting:** a fixed gain resistor on the amp. **Test:** phone SPL meter at the driver's ear | Low | Low (only once audio is fitted) |
| M19 | **Wiring:** insulation rated for its temperature, strain relief | ASTM 4.25.10.1, 4.25.10.8 ✔, EN 62115 cl. 14 ✔ | Yes | 🟢 Silicone wire, glands, ferrules, locking connectors | — | — | — |
| M20 | **Nuisance tripping:** fuses and cut-outs don't trip in normal use | ASTM 4.25.10.3 ✔ | Yes | 🔴 Untested until the car drives (300 ms over-current hold, fuse sizes) | **Test:** phase 6, full-load starts uphill with a sandbag; no `OVERCUR` and no blown fuse | Low | Low |
| M21 | **Labels and instructions:** adult supervision, no public roads, helmet, max weight | EN 62115 7.5, EN 71-1 7.10.3 ✔, ASTM 5, 6 ✔ | Yes | 🔵 Stock labels stay. SAFETY.md has the rules | Add one label on our box: "24 V, adult service only" | Low | Low |
| M22 | **Chemicals, plastics flammability** | EN 71-2/-3, SI 562, ASTM 4.2–4.3 ✔ | Yes | 🔵 Unchanged | — | — | — |

## Optional items: beyond what any factory does

| # | Item | Why it's optional | Status | Recommendation |
| --- | --- | --- | --- | --- |
| O1 | E-stop and contactor in the coil chain | No standard asks for either; the stock car has an on/off switch | 🟢 Designed, parts ordered | Keep: it's our M14 backbone. Phase 3 gate stays |
| O2 | Contactor weld check (`K1_WELD`), `NO_BUS`, bus sense and bleed | Stock has no contactor, so nothing to weld | 🟢 Code done (PR #52) | **Non-gating.** Test when convenient; if it misbehaves, turn it off rather than block the car |
| O3 | Current calibration against a clamp meter | Stock uses a fixed fuse; nobody calibrates | Open (#15, #29) | **Non-gating once M3's motor fuses are in.** Until then it's our only stall protection, so it gates |
| O4 | Stopping distance measured at each cap | EN 71-1 tests a parked hold (M2), not stopping distance | Phase 6 gate | **Non-gating, but do it:** it costs 10 minutes in the same session as M1 |
| O5 | Link loss stops within 0.5 s | Annex J only says it must stop | Phases 2, 4, 6 | Keep the test (it's free), drop the 0.5 s pass/fail number. "Stops" is the gate |
| O6 | Latched faults; no restart while the pedal is held | Beyond the standard ◐ | 🟢 Done | Keep; costs nothing |
| O7 | Box over-temperature cut-out (`HOT`), fans | Stock has none; M6 is the mandatory part | 🟢 Code done | Keep, non-gating |
| O8 | Battery under-voltage cut-off | Lead-acid has no requirement; lithium has its BMS | 🟢 Done | Keep |
| O9 | EMC: motor noise resets or misreads (#35) | Not in the ride-on reports. Our design fails safe: a reset stops the car | Open | **Non-gating:** a 5-minute full-load reversal test, and only fix if resets happen |
| O10 | Tilt and crash cut-out (#40) | No standard | Open | Phase 7 feature |
| O11 | I²C single point of failure (#36) | No standard; a lost bus already faults to a stop (`CUR_SENS`, `INPUT`) | Open | Later |
| O12 | Range test and packet-rate tuning (#37) | Annex J only needs the stop (M11) | Open | Do it, but non-gating |
| O13 | Per-wheel motor fault detection (#44) | No standard; M3 covers the fire risk | Open | Phase 7 |
| O14 | Wheel-speed km/h limit with halls (#41) | M1 is met by a fixed cap | Open | Later |
| O15 | Regen spike TVS (D2) | Protects our drivers, not the child | TBD | After the battery label |

## How to approach each open mandatory item

| Fix | Covers | Code | New parts | Redesign / test |
| --- | --- | --- | --- | --- |
| **A. Read two labels** | M10, M8 | Set `BATTERY_*` | A 30 A relay only if the key switch is under about 10 A | — |
| **B. km/h caps** | M1 | Set `KID_CAP_MIN/MID/MAX` and `PARENT_CAP` from measured speeds; keep Kid low as the power-on default | — | Phase 6 measurement. **Your call:** the child's age decides 6 or 8.2 km/h |
| **C. Per-motor fuses** | M3, M4, and makes O3 non-gating | Optional I²t later | 4 inline blade fuse holders (12 AWG) + 15 A and 20 A blades. Add to BOM via gen/tables.py | One fuse in each motor lead at the box edge; schematic gets F11–F14 |
| **D. Roll-away** | M2 | If the test fails: hold the low-side brake while parked in Locked | Only if it still fails with the car off: 4 × 24 V 40 A SPDT automotive relays (coil from MOTOR_24V; NC shorts the motor) | Test on a 10° slope first; the stock gearbox may already hold |
| **E. Heat tests** | M5, M6, M20 | — | IR thermometer (optional) | New phase 5 and phase 6 gate lines |
| **F. Box fire rating** | M7 | — | UL94 V-0 box, or a 1–2 mm aluminium plate | Box choice in the BOM |
| **G. Edges and labels** | M17, M21 | — | One printed label | Phase 5 checklist line |
| **H. Loudness** | M18 | DFPlayer volume cap | Gain resistor on the amp | Only when audio goes in (#46, #47) |

## Priority order for the open safety items

Ranked by concern first, then by how easy the fix is.

| Rank | Item | Concern | Complexity | Why here |
| --- | --- | --- | --- | --- |
| 1 | **A. Battery and key switch labels** (#13, #19) | High | Very low | Two photos. Unblocks battery thresholds, TVS and the key relay decision |
| 2 | **C. Per-motor fuses** | High | Low | The stalled-motor fire test is in every lab report; a few blade fuses fix it in hardware and remove a calibration gate. Add to the next order |
| 3 | **B. km/h caps** | High | Low | The legal limit for the age group; just numbers in config.h after one measurement session |
| 4 | **E. Locked-wheel and heat test** | High | Low | Proves C and M6. No parts |
| 5 | **D. Roll-away test** (fix only if it fails) | High | Low test, Medium fix | A 42 kg car with a child rolling away is the worst outcome on a slope |
| 6 | **F. Box fire rating** | Medium | Low | Swap one BOM line before the box is bought |
| 7 | **G. Edges, pinch points and label** | Medium | Low | A walk-round checklist in phase 5 |
| 8 | **M20 nuisance tripping** | Low | Low | Falls out of the phase 6 drive |
| 9 | **H. Loudness cap** | Low | Low | Only once audio is fitted |

## What changes in our gates if you agree

- **SAFETY.md "Before a child rides":** add M1 speed, M2 roll-away, M5 locked-wheel test; drop "within 0.5 s" (keep "stops"); mark current-vs-meter and stopping distance as recommended once the motor fuses are fitted.
- **COMMISSIONING.md:** phase 5 gets the locked-wheel, surface-temperature and edges checks; phase 6 gets speed per cap and the slope hold; the K1 weld and bus gates move to "recommended".
- **BOM:** per-motor fuses, a V-0 box, and the relays only if D fails.

## Sources

- [EN 71-1:2014 text (ride-on clause 4.15.1: speed, brakes, stability)](https://law.resource.org/pub/eu/toys/en.71.1.2014.html)
- [BS EN IEC 62115:2020+A11 preview (Annex J rationale, 13.1, clause 9)](https://webstore.ansi.org/preview-pages/bsi/preview_30420288.pdf)
- [IEC 62115 ed. 2.1 contents (Annex D, Annex J)](https://assets.vde-verlag.de/iec-normen/preview-pdf/info_iec62115%7Bed2.1%7Den.pdf)
- [TÜV Rheinland: EN IEC 62115:2020 changes](https://tuv.com/content-media-files/master-content/rs/Attachments\2286_Customer%20Information%202021-04_EN%20IEC%2062115%20Version%202020.pdf)
- [SGS: EU harmonizes EN IEC 62115](https://www.sgs.com/en-hk/news/2021/06/20210607-sgs-eu-harmonizes-en-iec-62115-for-electric-toys)
- [Eurofins: EU Toy Safety Regulation 2025/2509](https://www.eurofins.com/toys-hardlines/resources/articles/a-quick-overview-of-the-new-eu-toy-safety-regulation-eu-20252509/)
- [SII: SI 562 part 1, official standard page](https://www.sii.org.il/lobby/standardization/standard-page?id=1954)
- [SII: children's toys testing](https://www.sii.org.il/en/children-toys/)
- [ASTM F963-23 contents (4.25, 8.17, 8.18, 8.19)](https://www.boutique.afnor.org/en-gb/standard/astm-f96323/standard-consumer-safety-specification-for-toy-safety/am116367/366222)
- [Intertek test report, 12 V ride-on, ASTM F963-17 (2021)](https://www.bedbathandbeyond.com/downloads/pdf/1434854534_dcd93045-dcfb-335a-9950-f9e241a63450.pdf)
- [Test report, children's ride-on car, ASTM F963-23 (2024)](https://www.bedbathandbeyond.com/downloads/pdf/1488871438_a25dcd5b-8cdb-38f1-bc81-e2ea709c4ab0.pdf)
- [CPSC letter to ASTM on ride-on speed limits (2022)](https://www.cpsc.gov/s3fs-public/11-2-22-Letter-to-ASTM-Ride-on-Toys.pdf)
- [CPSC: Power Wheels recall (overheating, failure to stop)](https://www.cpsc.gov/Recalls/1999/CPSC-Fisher-Price-Announce-Recall-to-Repair-Power-Wheels-Ride-On-Battery-Powered-Vehicles)
- [Intertek: CCC rules for toys and children's vehicles](https://www.intertek.com/consumer/insight-bulletins/revision-of-ccc-rules-for-toys-and-childrens-vehicles/)
