#include "Log.h"

#include <Arduino.h>
#include <stdarg.h>

namespace logx {

void begin(uint32_t baud) {
  Serial.setTxBufferSize(1024);
  Serial.begin(baud);
}

void line(const char* fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if (n > (int)sizeof(buf) - 3) n = sizeof(buf) - 3;
  buf[n++] = '\r';
  buf[n++] = '\n';
  if (Serial.availableForWrite() < n) return;  // drop rather than block
  Serial.write((const uint8_t*)buf, n);
}

bool hasRoom(int bytes) { return Serial.availableForWrite() >= bytes; }

bool every(uint32_t& lastMs, uint32_t periodMs) {
  uint32_t now = millis();
  if (now - lastMs < periodMs) return false;
  lastMs = now;
  return true;
}

}  // namespace logx
