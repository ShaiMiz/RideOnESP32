// One-time bench setup of the listen-only Bluetooth log module (HM-10 BLE).
//
// The module comes set to 9600 baud; the car's serial log runs at 115200.
// On the car the module's TXD is not wired, so it can only be set up here,
// on the desk, before it goes in the box:
//
//   module VCC -> 5V (VIN pin), GND -> GND
//   module RXD -> GPIO17 (CRSF_TX), module TXD -> GPIO16 (CRSF_RX)
//   the ELRS receiver unplugged; no drivers, no motors; no phone connected
//
// Open the serial monitor. The firmware finds the module's baud rate, names
// it RideOn, sets 115200 baud, restarts it and checks that it answers at
// 115200. Genuine HM-10s take commands with no line ending; clones
// (MLT-BT05, CC41-A) want CRLF, so both are tried. Every kill line stays LOW.
//
// Gate (docs/COMMISSIONING.md, phase 3): the last line reads "done".
#include <Arduino.h>

#include "pins.h"
#include "safety/KillLines.h"

namespace {

KillLines lines;
const uint32_t BAUDS[] = {9600, 19200, 38400, 57600, 115200};
constexpr uint32_t TARGET_BAUD = 115200;

// Sends `cmd`, with or without CRLF, and returns the reply.
String ask(const char* cmd, bool crlf, uint32_t waitMs = 800) {
  while (Serial2.available()) Serial2.read();
  Serial2.print(cmd);
  if (crlf) Serial2.print("\r\n");
  String reply;
  uint32_t start = millis();
  while (millis() - start < waitMs) {
    while (Serial2.available()) reply += (char)Serial2.read();
    delay(5);
  }
  reply.trim();
  return reply;
}

bool answers(uint32_t baud, bool& crlf) {
  Serial2.end();
  Serial2.begin(baud, SERIAL_8N1, pins::CRSF_RX, pins::CRSF_TX);
  delay(50);
  for (bool c : {false, true}) {
    if (ask("AT", c).indexOf("OK") >= 0) {
      crlf = c;
      return true;
    }
  }
  return false;
}

// The baud rate the module answers at now, or 0.
uint32_t findBaud(bool& crlf) {
  for (uint32_t b : BAUDS) {
    Serial.printf("trying %lu baud...\n", (unsigned long)b);
    if (answers(b, crlf)) return b;
  }
  return 0;
}

}  // namespace

void setup() {
  lines.begin();
  Serial.begin(115200);
  delay(500);
  Serial.println("bt_setup: HM-10 Bluetooth log module setup (wiring in src/bench/bt_setup.cpp)");

  bool crlf = false;
  uint32_t found = findBaud(crlf);
  if (!found) {
    Serial.println("no answer: check the wiring, and that no phone is connected (it only takes commands when idle)");
    return;
  }
  Serial.printf("module answers at %lu baud (%s)\n", (unsigned long)found, crlf ? "CRLF" : "no line ending");
  Serial.printf("version: %s\n", ask("AT+VERSION", crlf).c_str());
  Serial.printf("AT+NAMERideOn -> %s\n", ask("AT+NAMERideOn", crlf).c_str());
  // 4 = 115200 on the HM-10 and its common clones. It applies after a restart.
  Serial.printf("AT+BAUD4 -> %s\n", ask("AT+BAUD4", crlf).c_str());
  Serial.printf("AT+RESET -> %s\n", ask("AT+RESET", crlf).c_str());
  delay(1500);

  uint32_t now = findBaud(crlf);
  if (now == TARGET_BAUD) {
    Serial.println("module answers at 115200: done. On the phone, connect to RideOn from a BLE terminal app.");
  } else if (now) {
    Serial.printf("module now answers at %lu, not 115200. Note its version line above and ask for help.\n",
                  (unsigned long)now);
  } else {
    Serial.println("no answer after the restart: power the module off and on, then run this again");
  }
}

void loop() { delay(1000); }
