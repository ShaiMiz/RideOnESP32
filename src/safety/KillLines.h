// The three hardware kill lines. Each has an external 10k pull-down, so it
// is "off" whenever the ESP32 is not actively driving it HIGH: during boot,
// reset, flashing, a crash or a brown-out.
//
//   DRIVE_EN        R_EN + L_EN of all four drive boards. LOW = motors coast.
//                   Taking it LOW and HIGH again also clears a BTS7960
//                   over-temperature latch.
//   STEER_EN        R_EN + L_EN of the steering board.
//   CONTACTOR_HOLD  MOSFET in series with the contactor coil. LOW opens the
//                   contactor and cuts battery power to every driver. The key
//                   switch and E-stop cut the coil in hardware regardless.
#pragma once

class KillLines {
 public:
  // Call first in setup(): drives all three lines LOW before anything else.
  void begin();

  void setDrive(bool on);
  void setSteer(bool on);
  void setContactor(bool closed);
  void allOff();

  bool drive() const { return drive_; }
  bool steer() const { return steer_; }
  bool contactor() const { return contactor_; }

 private:
  bool drive_ = false;
  bool steer_ = false;
  bool contactor_ = false;
};
