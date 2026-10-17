// Short, rate-limited serial log lines that never stall the control loop.
// A line is dropped, not waited for, when the serial TX buffer is full.
#pragma once

#include <stdint.h>

namespace logx {

void begin(uint32_t baud);
void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// True at most once per `periodMs` for the given timer slot.
bool every(uint32_t& lastMs, uint32_t periodMs);

}  // namespace logx
