#pragma once
#include <Arduino.h>

namespace PizzaPanel {
  bool begin64x32(uint8_t brightness = 100);
  void showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright);
  void loop();

  // NEW: bottom 1px progress bar, no border
  void progressBarReset();             // call once before starting OTA
  void showBottomBarPercent(uint8_t percent);  // 0..100; draws only the delta
}
