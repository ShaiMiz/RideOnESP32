// The child's controls: the pedal on GPIO36 (VP) and the shifter, horn, E-stop
// sense and seat switch on the PCF8574 I2C expander.
#pragma once

#include <stdint.h>

class ManualInput {
 public:
  // Returns false if the PCF8574 does not answer. Kid mode is then refused,
  // Parent mode still works.
  bool begin();
  void update();

  float pedal() const { return pedal_; }          // 0 released .. 1 fully pressed
  int8_t direction() const { return direction_; } // +1 forward, -1 reverse, 0 neutral
  bool horn() const { return horn_; }
  bool estopPressed() const { return estop_; }
  bool seat() const { return seat_; }
  bool ok() const { return ok_; }                 // false: expander not answering

 private:
  float pedal_ = 0.0f;
  int8_t direction_ = 0;
  bool horn_ = false;
  bool estop_ = false;
  bool seat_ = false;
  bool ok_ = false;
};
