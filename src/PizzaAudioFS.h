#pragma once
#include <Arduino.h>

namespace PizzaAudioFS {
  void begin(int bclkPin, int lrckPin, int doutPin);
  bool playClip(uint8_t clipId, bool loop);
  bool playPath(const char* path, bool loop);
  void stop();
  void loop();
  void setVolume(uint8_t vol);
  bool isPlaying();
}

