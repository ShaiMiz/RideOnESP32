// Task watchdog on the loop task. A loop stuck for longer than
// cfg::WATCHDOG_MS resets the ESP32, and the kill-line pull-downs stop the car.
#pragma once

namespace watchdog {

// Call at the end of setup(), after any slow start-up work.
void begin();
// Call once per control tick.
void feed();

}  // namespace watchdog
