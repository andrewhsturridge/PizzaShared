#include "PizzaAudioFS.h"
#include <FS.h>
#include <LittleFS.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

namespace PizzaAudioFS {
  static AudioFileSourceLittleFS* s_file = nullptr;
  static AudioFileSourceBuffer*   s_buf  = nullptr;
  static AudioGeneratorWAV*       s_wav  = nullptr;
  static AudioOutputI2S*          s_out  = nullptr;

  static bool     s_loop = false;
  static uint8_t  s_lastClip = 0;
  static char     s_lastPath[48] = {0};
  static float    s_gain = 0.85f;   // 0.0..~1.2

  static void closeChain() {
    if (s_wav) { s_wav->stop(); delete s_wav; s_wav = nullptr; }
    if (s_buf) { delete s_buf;  s_buf  = nullptr; }
    if (s_file){ delete s_file; s_file = nullptr; }
  }

  void begin(int bclkPin, int lrckPin, int doutPin) {
    if (!LittleFS.begin()) LittleFS.begin(true);
    if (!s_out) {
      // External I2S pins, mono
      s_out = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S);
      s_out->SetPinout(bclkPin, lrckPin, doutPin);    // (BCLK, LRCK/WS, DOUT)
      s_out->SetChannels(1);                          // mono path
      s_out->SetGain(s_gain);
    }
  }

  static bool startPath(const char* path) {
    closeChain();
    if (!LittleFS.exists(path)) return false;

    s_file = new AudioFileSourceLittleFS(path);
    if (!s_file) return false;

    // 4 KB buffer smooths over brief CPU/DMA stalls. Bump to 8192 if still choppy.
    s_buf  = new AudioFileSourceBuffer(s_file, 4096);
    if (!s_buf) { closeChain(); return false; }

    s_wav = new AudioGeneratorWAV();
    if (!s_wav->begin(s_buf, s_out)) { closeChain(); return false; }
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
    closeChain();
  }

  void setVolume(uint8_t vol) {
    s_gain = (float)vol / 212.0f;        // vol≈200 → ~0.94 gain
    if (s_out) s_out->SetGain(s_gain);
  }

  bool isPlaying() {
    return s_wav && s_wav->isRunning();
  }

  void loop() {
    if (!s_wav) return;
    if (s_wav->isRunning()) {
      if (!s_wav->loop()) {
        if (s_loop && s_lastPath[0]) {
          startPath(s_lastPath);         // seamless loop
        } else {
          stop();
        }
      }
    } else {
      if (s_loop && s_lastPath[0]) startPath(s_lastPath);
      else stop();
    }
  }
}
