// One-time bench setup of the listen-only Bluetooth module (HC-06 class).
//
// The module comes set to 9600 baud; the car's serial log runs at 115200.
// On the car the module's TX is not wired, so it can only be set up here,
// on the desk, before it goes in the box:
//
//   module VCC -> 5V (VIN pin), GND -> GND
//   module RXD -> GPIO17 (CRSF_TX), module TXD -> GPIO16 (CRSF_RX)
//   the ELRS receiver unplugged; no drivers, no motors
//
// Open the serial monitor, type a 4-digit PIN and press Enter. The firmware
// finds the module's baud rate, sets its name to RideOn, the PIN, and
// 115200 baud, then checks that it answers at 115200. Every kill line stays
// LOW the whole time.
//
// Gate (docs/COMMISSIONING.md, phase 3): the last line reads "done".
#include <Arduino.h>

#include "pins.h"
#include "safety/KillLines.h"

namespace {

KillLines lines;
const uint32_t BAUDS[] = {9600, 19200, 38400, 57600, 115200};
constexpr uint32_t TARGET_BAUD = 115200;

// Sends `cmd` (with or without CRLF: HC-06 firmwares differ) and returns the reply.
String ask(const char* cmd, bool crlf, uint32_t waitMs = 1200) {
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
  for (bool c : {false, true}) {
    if (ask("AT", c).indexOf("OK") >= 0) {
      crlf = c;
      return true;
    }
  }
  return false;
}

String readPin() {
  Serial.println("type a 4-digit PIN for the module and press Enter:");
  String pin;
  for (;;) {
    if (!Serial.available()) {
      delay(10);
      continue;
    }
    char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (pin.length() == 4) return pin;
      Serial.println("4 digits, please:");
      pin = "";
    } else if (isdigit((unsigned char)ch) && pin.length() < 4) {
      pin += ch;
    }
  }
}

}  // namespace

void setup() {
  lines.begin();
  Serial.begin(115200);
  delay(500);
  Serial.println("bt_setup: Bluetooth module setup (wiring in src/bench/bt_setup.cpp)");
  String pin = readPin();

  uint32_t found = 0;
  bool crlf = false;
  for (uint32_t b : BAUDS) {
    Serial.printf("trying %lu baud...\n", (unsigned long)b);
    if (answers(b, crlf)) {
      found = b;
      break;
    }
  }
  if (!found) {
    Serial.println("no answer: check the wiring, and that no phone is connected (it only answers AT when unpaired)");
    return;
  }
  Serial.printf("module answers at %lu baud (%s)\n", (unsigned long)found, crlf ? "CRLF" : "no line ending");

  String name = crlf ? "AT+NAME=RideOn" : "AT+NAMERideOn";
  String pinCmd = crlf ? "AT+PSWD=" + pin : "AT+PIN" + pin;
  const char* baudCmd = crlf ? "AT+UART=115200,0,0" : "AT+BAUD8";
  Serial.printf("%s -> %s\n", name.c_str(), ask(name.c_str(), crlf).c_str());
  Serial.printf("PIN -> %s\n", ask(pinCmd.c_str(), crlf).c_str());
  Serial.printf("%s -> %s\n", baudCmd, ask(baudCmd, crlf).c_str());

  delay(500);
  bool c = crlf;
  if (answers(TARGET_BAUD, c)) {
    Serial.println("module answers at 115200: done. Pair a phone with RideOn and the new PIN.");
  } else {
    Serial.println("module does not answer at 115200: power it off and on, then run this again");
  }
}

void loop() { delay(1000); }
