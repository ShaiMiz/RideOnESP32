# Bluetooth log module (listen-only)

A small HM-10 Bluetooth Low Energy module (U33) lets a phone read the car's serial log on the spot, without a laptop or a USB cable. Shai chose a listen-only module on 26 Oct 2026, over a phone app or the ESP32's own Bluetooth, and BLE on 28 Oct so it works with Shai's iPhone and a child's Android phone.

## Why it can't control anything

- **Listen-only by wiring.** The module's RXD hears the ESP32's TX0, the same line the USB serial log uses. Its TXD is **not connected**, so nothing sent from a phone, by anyone, reaches the ESP32.
- **The ESP32's own Wi-Fi and Bluetooth stay off**, as [SAFETY.md](../../SAFETY.md) requires.
- A 1k series resistor (R29) keeps a dead or shorted module from holding down the USB serial line, so flashing and the USB log still work.
- There is no PIN: HM-10 pairing PINs are unreliable on iPhones and on the clones. Anyone within about 10 m can read the log while the car is on. The log holds no personal data, only modes, faults and readings.

## Parts and wiring

| Ref | Part | Connects |
| --- | --- | --- |
| U33 | HM-10 (TI CC2541) on a 6-pin breakout (STATE, VCC, GND, TXD, RXD, BRK), 3.6-6 V in | VCC to A1 5V (5V_LOGIC), GND to A1 GND, RXD through R29. TXD, STATE, BRK not connected |
| R29 | 1k resistor, on A1 | ESP32 R3 (TX0, net `LOG_TX`) to U33 RXD (net `BT_RXD`) |

Tape off U33's TXD or cut its header pin. Details are in [kicad/WIRE_LIST.md](kicad/WIRE_LIST.md) and on the controller sheet of the schematic. DSD TECH sells the genuine HM-10; the MLT-BT05 and CC41-A clones work too.

## Phones

BLE works on both:

| Phone | App | Connect |
| --- | --- | --- |
| Android | **Serial Bluetooth Terminal** (Kai Morich) | Devices, then the Bluetooth LE tab, then scan, then `RideOn` |
| iPhone | A BLE terminal that supports the HM-10 (search the App Store for "HM-10 terminal"; it must use the FFE0/FFE1 service). **LightBlue** (Punch Through) is a fallback: open `RideOn`, then FFE1, and tap Listen for notifications | Scan, then `RideOn` |

Only one phone can be connected at a time. A BLE link passes on roughly 2 KB/s, a little less than a busy log, so the live log can drop a line now and then. The report below is paced so it arrives whole.

## One-time setup on the bench

The module ships at 9600 baud; the car logs at 115200. Its TXD isn't wired on the car, so it is set up before it goes in the box, with the `bt_setup` bench firmware:

1. Wire the module to a bare ESP32 (no drivers, receiver unplugged): VCC to VIN (5 V), GND to GND, module RXD to GPIO17, module TXD to GPIO16. No phone connected.
2. `pio run -e bt_setup -t upload`, then `pio device monitor`.
3. It finds the module's baud rate, names it `RideOn`, sets 115200 baud, restarts it and checks that it answers at 115200. The last line reads `done`.
4. Flash `car` again and fit the module as wired above.

## Reading the log on the phone

1. Connect to `RideOn` from the app (table above).
2. The live log scrolls: the status line every 500 ms, and every event as it happens ([firmware/LOG_EVENTS.md](../firmware/LOG_EVENTS.md)).
3. **Full report:** with the car Locked or Killed and stopped, hold the horn for 3 seconds (the kid's horn button, or the radio's horn switch). The console prints the status, the fault table, the crash recorder copy and the last 50 log records, one line every 100 ms so the BLE link keeps up (about 15 s). On USB, typing `report` prints the same at full speed.
4. Save or share the session from the app and attach it to the issue.

The report only prints. A horn held while driving never triggers it, and a hold must start with the car already parked.
