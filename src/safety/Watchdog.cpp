#include "Watchdog.h"

#include <esp_task_wdt.h>

#include "config.h"

namespace watchdog {

void begin() {
  esp_task_wdt_config_t wdt = {};
  wdt.timeout_ms = cfg::WATCHDOG_MS;
  wdt.idle_core_mask = 0;
  wdt.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&wdt) != ESP_OK) esp_task_wdt_init(&wdt);
  esp_task_wdt_add(nullptr);  // the calling task: the Arduino loop
}

void feed() { esp_task_wdt_reset(); }

}  // namespace watchdog
