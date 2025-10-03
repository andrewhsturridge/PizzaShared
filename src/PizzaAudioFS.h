#pragma once
#include <Arduino.h>

namespace PizzaAudioFS {
  // Call once in setup() after WiFi/FS init. Pins are your I2S pins.
  void begin(int bclkPin, int lrckPin, int doutPin);

  // Play "/clips/%03u.wav" (e.g., 005.wav). If loop=true, auto-restart on EOF.
  bool playClip(uint8_t clipId, bool loop);

  // Play an explicit file path (e.g., "/clips/intro.wav")
  bool playPath(const char* path, bool loop);

  // Stop any playback immediately.
  void stop();

  // Drive the decoder; call every loop()
  void loop();

  // 0..255 (mapped to I2S gain). 200≈loud, 80≈quiet
  void setVolume(uint8_t vol);
}