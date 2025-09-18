#pragma once
#include <Arduino.h>

namespace PizzaPanel {
  // Returns true on success; shows a quick border proof-of-life at boot
  bool begin64x32(uint8_t brightness = 100);

  // style: 0=scroll, 1=static, 2=wipe (stub), 3=border blink (stub)
  void showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright);

  // Call every loop() for animations (scroll)
  void loop();
}
