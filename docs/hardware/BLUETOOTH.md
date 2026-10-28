# Bluetooth log module (listen-only)

A small HC-06 Bluetooth serial module (U33) lets a phone read the car's serial log on the spot, without a laptop or a USB cable. Shai chose it on 26 Oct 2026 over a phone app or the ESP32's own Bluetooth.

## Why it can't control anything

- **Listen-only by wiring.** The module's RXD hears the ESP32's TX0, the same line the USB serial log uses. Its TXD is **not connected**, so nothing typed on the phone, by anyone, reaches the ESP32.
- **The ESP32's own Wi-Fi and Bluetooth stay off**, as [SAFETY.md](../../SAFETY.md) requires.
- A 1k series resistor (R29) keeps a dead or shorted module from holding down the USB serial line, so flashing and the USB log still work.
- The worst a stranger nearby can do is read the log. The module has a PIN, set during setup.

## Parts and wiring

| Ref | Part | Connects |
| --- | --- | --- |
| U33 | HC-06 module, 4 pins (VCC, GND, TXD, RXD), 3.6-6 V carrier | VCC to A1 5V (5V_LOGIC), GND to A1 GND, RXD through R29 |
| R29 | 1k resistor, on A1 | ESP32 R3 (TX0, net `LOG_TX`) to U33 RXD (net `BT_RXD`) |

U33 TXD: tape it off or cut the header pin. Details in [kicad/WIRE_LIST.md](kicad/WIRE_LIST.md) and the controller sheet of the schematic.

**Phones:** HC-06 is Bluetooth Classic (SPP), which Android supports and iPhone does not. For an iPhone, an HM-10 class BLE module wires the same way but needs a BLE terminal app; ask before buying one.

## One-time setup on the bench

The module ships at 9600 baud; the car logs at 115200. Its TXD isn't wired on the car, so it is set up before it goes in the box, with the `bt_setup` bench firmware:

1. Wire the module to a bare ESP32 (no drivers, receiver unplugged): VCC to VIN (5 V), GND to GND, module RXD to GPIO17, module TXD to GPIO16. No phone paired.
2. `pio run -e bt_setup -t upload`, then `pio device monitor`.
3. Type a 4-digit PIN and press Enter. It finds the module's baud rate, sets the name `RideOn`, the PIN and 115200 baud, and checks the module answers at 115200. The last line reads `done`.
4. Flash `car` again and fit the module as wired above.

## Reading the log on the phone

1. Android: install **Serial Bluetooth Terminal** (Kai Morich), pair with `RideOn` using the PIN, and connect from the app's Devices menu.
2. The live log scrolls: the status line every 500 ms, and every event as it happens ([firmware/LOG_EVENTS.md](../firmware/LOG_EVENTS.md)).
3. **Full report:** with the car Locked or Killed and stopped, hold the horn for 3 seconds (the kid's horn button, or the radio's horn switch). The console prints the status, the fault table, the crash recorder copy and the last 50 log records. On USB, typing `report` does the same.
4. Save the session in the app (Share or Save) and attach it to the issue.

The report only prints. A horn held while driving never triggers it, and a hold must start with the car already parked.
