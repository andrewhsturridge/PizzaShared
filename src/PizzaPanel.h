// PizzaPanel.h  (add the note; no signature change)
#pragma once
#include <Arduino.h>

namespace PizzaPanel {
  bool begin64x32(uint8_t brightness = 100);
  
  // Text color (RGB 0..255); brightness still honored by s_bright
  void setColor(uint8_t r, uint8_t g, uint8_t b);

  // style: 0 = horizontal marquee, 1 = static, 2 = wrapped vertical (auto static/scroll), 3 = single-line vertical marquee
  void showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright);
  
  void setWeight(uint8_t weight);  // 0=normal, 1=bold, 2=extra bold

  // Call every loop() for animations
  void loop();

  // Optional OTA bar
  void progressBarReset();
  void showBottomBarPercent(uint8_t percent);
}
