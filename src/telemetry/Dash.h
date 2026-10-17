// 0.91in SSD1306 OLED on the dash: mode, battery, fault code. (later: features)
#pragma once

class Dash {
 public:
  bool begin() { return true; }
  void update(const char* /*mode*/, float /*volts*/, const char* /*fault*/) {}
};
