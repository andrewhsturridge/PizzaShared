#pragma once
#include <Arduino.h>

namespace PizzaAudio {
  bool beginI2S();                   // BCLK=43, LRCLK=44, DIN=12
  void playClip(const int16_t* pcm, size_t samples, uint8_t vol=255); // simple, blocking for now
}
