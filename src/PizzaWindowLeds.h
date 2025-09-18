#pragma once
#include <Arduino.h>

namespace PizzaWindowLeds {
  bool begin(uint8_t pin, uint16_t count);
  uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);  // <-- new helper
  void blink(uint32_t color, uint16_t msOn, uint16_t msOff);
  void loop();
}
