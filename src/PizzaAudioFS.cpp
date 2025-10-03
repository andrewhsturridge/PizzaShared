#include "PizzaAudioFS.h"
#include <FS.h>
#include <LittleFS.h>
#include <AudioFileSourceFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

namespace PizzaAudioFS {
  static AudioFileSourceFS* s_src = nullptr;
  static AudioGeneratorWAV* s_wav = nullptr;
  static AudioOutputI2S*    s_out = nullptr;

  static bool     s_loop = false;
  static uint8_t  s_lastClip = 0;
  static char     s_lastPath[48] = {0};
  static float    s_gain = 0.8f;  // 0.0..4.0 in ESP8266Audio; we’ll map 0..255 below.

  static void closeAll() {
    if (s_wav) { s_wav->stop(); delete s_wav; s_wav = nullptr; }
    if (s_src) { delete s_src; s_src = nullptr; }
  }

  void begin(int bclkPin, int lrckPin, int doutPin) {
    if (!LittleFS.begin()) LittleFS.begin(true);
    if (!s_out) {
      s_out = new AudioOutputI2S();
      s_out->SetPinout(bclkPin, lrckPin, doutPin); // (BCLK, LRCK/WS, DOUT)
      s_out->SetGain(s_gain);
    }
  }

  static bool startPath(const char* path) {
    closeAll();
    if (!LittleFS.exists(path)) return false;
    s_src = new AudioFileSourceFS(LittleFS, path);
    s_wav = new AudioGeneratorWAV();
    if (!s_wav->begin(s_src, s_out)) {
      closeAll();
      return false;
    }
    return true;
  }

  bool playPath(const char* path, bool loop) {
    s_loop = loop;
    strlcpy(s_lastPath, path, sizeof(s_lastPath));
    s_lastClip = 0;
    return startPath(path);
  }

  bool playClip(uint8_t clipId, bool loop) {
    char path[32];
    snprintf(path, sizeof(path), "/clips/%03u.wav", (unsigned)clipId);
    s_lastClip = clipId;
    s_loop = loop;
    strlcpy(s_lastPath, path, sizeof(s_lastPath));
    return startPath(path);
  }

  void stop() {
    s_loop = false;
    s_lastClip = 0;
    s_lastPath[0] = '\0';
    closeAll();
  }

  void setVolume(uint8_t vol) {
    // Map 0..255 -> ~0.0..1.2 (tweak to taste)
    s_gain = (float)vol / 212.0f;
    if (s_out) s_out->SetGain(s_gain);
  }

  void loop() {
    if (s_wav) {
      if (s_wav->isRunning()) {
        if (!s_wav->loop()) {
          // EOF or error
          if (s_loop && s_lastPath[0]) {
            startPath(s_lastPath); // restart same clip/path
          } else {
            stop();
          }
        }
      } else {
        if (s_loop && s_lastPath[0]) {
          startPath(s_lastPath);
        } else {
          stop();
        }
      }
    }
  }
}