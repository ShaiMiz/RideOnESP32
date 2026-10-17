# Ride-on car safety standards vs our design

17 Oct 2026. Car: Can-Am Maverick X3 24 V 4WD (kolbosima item 8419715), stock controller HY2005RX-24V/40A. Our design: SAFETY.md, POWER_CHAIN.md, COMMISSIONING.md, CONTROL_MODES.md, `include/config.h` on main, draft schematic v0.2, issues #1–#47.

**Verdict colours:** 🟢 above, 🔴 below or unverified, 🔵 same.

**Confidence key:** ✔ = checked in a source listed at the bottom. ◐ = from general knowledge of these standards, not checked against the paid standard text; verify before relying on the exact number.

## Short answer

- **Yes, we can know.** Chinese factories don't publish their own test plans, but every car sold in Israel, the EU or the US has to pass a fixed set of public standards, tested once per model by a lab (SGS, Intertek, BV, TÜV), plus the factory's own per-unit checks. The test report for *our* model exists and the importer has it (see "How to find out for our exact car").
- **Where we're above the factory:** loss-of-signal stop, parent override, latched faults, a hardware E-stop and contactor, per-motor current monitoring, battery cut-off, tilt cut-out (planned) and a staged commissioning plan. A stock car has a fuse, a 40 A controller and a cheap 2.4 GHz remote.
- **Where we're the same:** everything mechanical and chemical (frame, seat, stability, plastics, battery), because we don't change it.
- **Where we fall below (8 gaps):** the car rolls freely after a kill or when parked, speed is capped as a % of motor power and not in km/h, a single stalled motor has no hardware backstop below 40 A, the radio may exceed the legal 100 mW, plus audio loudness, the enclosure's fire rating, EMC, and no stall or heating test in commissioning. Details and fixes are in the "Gaps" section.

## Which standards apply

| Market | Mandatory for a 24 V ride-on | Who tests |
| --- | --- | --- |
| **EU (CE)** | Toy Safety Directive 2009/48/EC via **EN 71-1** (mechanical, incl. ride-on clause 4.15) ✔, EN 71-2 (flammability), EN 71-3 (chemicals), **EN IEC 62115:2020+A11** (electric toys) ✔. Radio remote: RED 2014/53/EU (EN 300 328, EN 301 489) ◐. RoHS, REACH. | Type test by a lab; manufacturer signs the Declaration of Conformity |
| **Israel** | **SI 562** (toy safety, based on EN 71: mechanical, flammability, heavy metals, phthalates) ✔, enforced on import by the Standards Institution (SII) lab ✔. Electric toys: SI 62115 (the Israeli adoption of IEC 62115) ◐. Radio remote: Ministry of Communications import approval ◐. | SII lab tests samples or accepts a foreign lab report; customs release needs SII approval |
| **US** | **ASTM F963** (mandatory under CPSIA; covers battery-powered ride-ons) ✔, 16 CFR 1500 hazards, FCC Part 15 for the remote. Children's Product Certificate from a CPSC-accepted lab. | Third-party lab, every model |
| **China (domestic sales)** | **CCC**: ride-on toys and electric toys, to GB 6675.1–.4 and **GB 19865** (= IEC 62115, applies to powered ride-ons) ✔ | Type test + post-certification sampling at the factory ✔ |

All four boil down to the same two documents: **EN 71-1 / ISO 8124-1 (mechanics)** and **IEC 62115 (electrics)**. Export factories test to both once per model.

## What the factory actually does

1. **Type test, once per model** (and again on design changes or a retailer's yearly retest): the full EN 71 / EN 62115 / ASTM F963 test list below, run by a lab on a few samples. This is where speed, brakes, stability, stalled motors and short circuits are tested.
2. **Per-unit production checks** ◐: incoming checks on batteries, chargers and controllers; a run test of each car on the line (forward, reverse, each speed, remote pairing, lights and sounds); visual and fastener checks.
3. **Pre-shipment inspection** ◐: the buyer's inspector (QIMA, SGS, V-Trust) pulls samples by AQL, runs function, remote-range and speed checks, and checks labels, warnings and carton drop.

So the deep safety tests happen on paper, once. Nobody re-tests your individual car's stall behaviour.

## Test-by-test comparison

| # | Requirement | Standard | Stock car (likely) | Our design | Verdict |
| --- | --- | --- | --- | --- | --- |
| 1 | **Max speed.** Age 3–6: ≤ 6 km/h, or ≤ 8.2 km/h if a two-position limiter can hold it to ≤ 6 km/h and ships in the low position. Age 6+: ≤ 16 km/h | EN 71-1 4.15.1.8, EN 62115 13.10 ✔ | 4.5 / 6 / 7.5 km/h by gear (listing) | Caps are % of motor power: Kid 30/60/100 %, **Parent 100 %**. No km/h figure anywhere | **🔴 Below until measured.** 100 % may match or beat the stock top speed |
| 2 | **Brakes / roll-away.** Ride-ons with free-wheeling must brake; ≥ 30 kg must lock a brake; must not move > 5 cm on the brake test ramp. Motor braking counts if the car creeps ≤ 0.36 m/s on the ramp | EN 71-1 4.15.1.5, test 8.26.1 ✔ | Gearbox + controller shorts the motors when stopped ◐ | Locked mode brakes, then **drops drive enable, so the motors free-wheel**. Kill, E-stop, key-off and contactor-open all coast | **🔴 Below.** Car (42 kg + child) can roll on a slope once stopped or killed |
| 3 | **Stability:** must not tip on the test slope with a child-mass load | EN 71-1 4.15.1.4, test 8.23.1 ✔ | Passed at type test | Unchanged; tilt cut-out planned (#40) | 🔵 Same, then 🟢 above |
| 4 | **Static and dynamic strength** of seat and frame | EN 71-1 4.15.1.3 ✔ | Passed | Unchanged; our box must be bolted so it can't come loose | 🔵 Same |
| 5 | **Max 24 V** supply, higher internal voltages only if inaccessible | EN 62115 13.1 ✔ | 24 V | Same battery | 🔵 Same |
| 6 | **Locked-motor test:** wheels locked, run to steady state (30 s if a hold-to-run switch drives it); accessible parts stay under temperature limits; nothing catches fire | EN 62115 clause 9 ✔ | Fuse + 40 A controller limit | 40 A fuse; software over-current per motor 20 A / total 35 A for 300 ms; driver fault flag; box DS18B20 cut-out (#34); stall detection (#44) | **🟢 Above on detection, 🔴 below on backstop:** one stalled 24 V 555-size motor likely draws under 20 A, so software never trips and the 40 A fuse never blows. Not tested in commissioning |
| 7 | **Short-circuit tests** across live parts, then temperature limits | EN 62115 clause 9 ✔ | Fuse | 40 A main fuse within 15 cm, a fuse per rail (F2–F8) | 🔵 Same or 🟢 above |
| 8 | **Protective electronic circuits:** if safety relies on electronics, it must still be safe when that electronics fails ◐ | EN 62115 Annex D ✔ (title) | Simple | Hardware layer (fuse, key, E-stop, contactor, pull-downs) stays when the ESP32 fails | 🟢 Above for stopping; 🔴 same gap as #6 for overheating |
| 9 | **Remote control for ride-ons:** car stops automatically on leaving range; a mode without the remote must need an adult and a tool | EN 62115 Annex J ✔ | Cheap 2.4 GHz remote, override and stop button | Link loss (200 ms, LQ < 40 %) brakes to a stop; no radio = no driving in any mode; parent brake overrides the pedal; kill switch; binding phrase + Model Match | **🟢 Above** |
| 10 | **Battery:** rechargeables to IEC 62133 (NiMH, lithium; lead-acid is outside it) ◐; no leaking in any position; not removable without two actions or a tool | EN 62115 13.4, 15.5 ✔ | Stock pack | Same pack; under-voltage cut-off at 21 V with hysteresis; chemistry still unknown (#13) | 🔵 Same, 🟢 above on cut-off. If it turns out to be lithium, the pack's own BMS must stay |
| 11 | **Charger:** toy-safe charger, marked "use only with" | EN 62115 7.2.3 ✔ | Stock charger | Unchanged, as long as the charge port stays wired straight to the battery | 🔵 Same |
| 12 | **Wiring:** protected against sharp edges, strain relief, screw connections hold | EN 62115 clauses 14, 16 ✔ | Basic | Glands, ferrules, locking connectors, 10/12/14 AWG | 🟢 Above |
| 13 | **Heat and fire:** insulating parts near live connections pass glow-wire / needle-flame tests ◐ | EN 62115 clause 18, Annex B ✔ | Flame-rated controller case ◐ | Plain ABS box (BOM) | **🔴 Below** unless the box is flame-retardant (UL94 V-0) or metal |
| 14 | **EMC immunity** (e.g. IEC 61000-4-3 radiated fields) | EN 62115 refs ✔, EMC Directive | Type-tested | Untested; motor noise resets are an open risk (#35) | **🔴 Unverified** |
| 15 | **Radio power:** 2.4 GHz licence-free limit 100 mW EIRP (EU EN 300 328; Israel follows the same limit ◐) | RED / MoC ◐ | ~1–10 mW remote | ELRS can run 250 mW+ | **🔴 Below if left at max.** Set ≤ 100 mW (#21, #37) |
| 16 | **Loudness:** continuous toy sound about 85 dB(A) max ◐ | EN 71-1 4.20 ◐ | Small speaker | Planned 2 × 50 W BT amp (#47) + DFPlayer (#46) | **🔴 Below if uncapped** |
| 17 | **LEDs:** optical radiation limits | EN 62115 Annex E ✔ | Small LEDs | LED strips through PCA9685 (#45) | 🔵 Same, if ordinary strips |
| 18 | **Warnings and manual:** adult supervision, no public roads, protective gear, max weight | EN 62115 7.5, EN 71-1 7.10.3 ✔ | Labels + manual | SAFETY.md rules (helmet, parent holds radio) | 🔵 Same; keep the stock labels on the car |
| 19 | **Chemicals / flammability of plastics** | EN 71-2/-3, SI 562 ✔ | Tested | Unchanged | 🔵 Same |

## Gaps, with suggested fixes

1. **Roll-away after stop, kill or E-stop (row 2).** Add a parking brake that works without power: an automotive relay per motor pair whose *normally-closed* contacts short the motor terminals when the contactor is open (classic dynamic brake), or check that the gearbox alone holds. Gate test: car + 35 kg sandbag on a 10° slope, killed, must not move more than 5 cm. Add to phase 3/5.
2. **Speed in km/h (row 1).** In phase 6 measure the top speed at 100 % and set `KID_CAP_*` so Kid-high is ≤ 6 km/h (or ≤ 8.2 km/h with the switch defaulting low) and decide a Parent cap. Later, use the wheel halls (#41) to enforce km/h directly.
3. **Single stalled motor (row 6).** Add a hardware backstop per motor: a 15–20 A blade fuse or PTC in each motor lead, sized after reading the motor label (#14). In firmware, an I²t (heat-over-time) limit per motor rather than only the 20 A threshold, which ties into #44.
4. **Stall and heat test (rows 6, 7).** Add to phase 4/5: lock one wheel, hold full throttle 30 s, then until something trips; record motor, driver, wire and box-surface temperatures. This is the factory's clause 9 test.
5. **Radio power (row 15).** Set the ELRS TX max power to 100 mW and use the LBT (EU) regulatory build. Range on a playground is far less than 100 mW covers; #37 confirms it.
6. **Enclosure fire rating (row 13).** Buy a UL94 V-0 or metal box, or line the board area with a metal plate.
7. **Audio loudness (row 16).** Cap the amp's volume in hardware (a fixed resistor or a gain jumper) to keep about 85 dB(A) at the driver's ear.
8. **EMC (row 14).** Already #35; add a gate: full-load reversals and radio keyed next to the box with no resets or false readings in the log.

Gaps 1–4 are safety items; 5–8 are compliance and comfort.

## How to find out for our exact car

- Look on the car, box and manual for the **CE mark**, a lab report number, and the **SII import sticker** (Israeli standard mark with the importer's name). Photograph them; that can join the phase 1 photo list.
- Ask Kolbo Sima (the importer) for the **test report** for item 8419715 (EN 71-1 / EN 62115, often by SGS, BV or Intertek). Israeli importers need it for SII customs release, so it exists.
- With the report, we can fill in the stock car's measured values (top speed, ramp result, stall temperatures) and replace the "likely" column above with facts.

## A note on certification

Once the stock controller is replaced, the car no longer matches what was tested, so its CE/SII approval no longer covers it. That's fine for a private build, but it's why gaps 1–4 matter: nobody else has tested this version, and the commissioning gates are our type test.

## Sources

- [EN 71-1:2014 text (ride-on clause 4.15.1)](https://law.resource.org/pub/eu/toys/en.71.1.2014.html)
- [TÜV Rheinland: EN IEC 62115:2020 changes](https://tuv.com/content-media-files/master-content/rs/Attachments\2286_Customer%20Information%202021-04_EN%20IEC%2062115%20Version%202020.pdf)
- [BS EN IEC 62115:2020+A11 preview (Annex J, 13.1, clause 9 rationale)](https://webstore.ansi.org/preview-pages/bsi/preview_30420288.pdf)
- [IEC 62115 ed. 2.1 table of contents (VDE preview)](https://assets.vde-verlag.de/iec-normen/preview-pdf/info_iec62115%7Bed2.1%7Den.pdf)
- [SGS: EU harmonizes EN IEC 62115](https://www.sgs.com/en-hk/news/2021/06/20210607-sgs-eu-harmonizes-en-iec-62115-for-electric-toys)
- [CPSC letter to ASTM on ride-on speed limits (2022)](https://www.cpsc.gov/s3fs-public/11-2-22-Letter-to-ASTM-Ride-on-Toys.pdf)
- [CPSC: Power Wheels recall, overheating and failure to stop](https://www.cpsc.gov/Recalls/1999/CPSC-Fisher-Price-Announce-Recall-to-Repair-Power-Wheels-Ride-On-Battery-Powered-Vehicles) (10 million cars, ~150 fires, 71 reports of cars not stopping: the two failure modes our design targets)
- [Intertek: CCC rules for toys and children's vehicles](https://www.intertek.com/consumer/insight-bulletins/revision-of-ccc-rules-for-toys-and-childrens-vehicles/)
- [SII: children's toys testing](https://www.sii.org.il/en/children-toys/)
- [ComplianceGate: US toy standards](https://compliancegate.com/toy-safety-standards-united-states)
- [TradeAider: ride-on stability and brake testing](https://www.tradeaiders.com/ru/ride-on-toys-testing-safety-stability-braking-performance-guide.html)
