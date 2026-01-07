#include "PizzaAudioFS.h"
#include <FS.h>
#include <LittleFS.h>

// ESP8266Audio (works on ESP32 too)
#include <AudioFileSourceLittleFS.h>
#include <AudioFileSourceBuffer.h>
#if __has_include(<AudioFileSourcePROGMEM.h>)
#  include <AudioFileSourcePROGMEM.h>
#  define PZ_AUDIO_HAVE_PROGMEM_SRC 1
#else
#  define PZ_AUDIO_HAVE_PROGMEM_SRC 0
#endif
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/semphr.h>
  #include <esp32-hal-psram.h>
#endif

namespace PizzaAudioFS {

  
#if PZ_AUDIO_HAVE_PROGMEM_SRC
  using PzMemSrcT = AudioFileSourcePROGMEM;
#else
  // Fallback: no PROGMEM source available in this ESP8266Audio version.
  // RAM caching will be disabled and clips will stream from LittleFS.
  class PzMemSrcT;
#endif

// --------------------------
  // Tuning knobs
  // --------------------------
  // Streaming buffer used when we cannot (or choose not to) preload a clip into RAM.
  static constexpr size_t STREAM_BUFFER_BYTES = 16384;

  // If a WAV file is <= this size, we try to preload it into RAM/PSRAM and then play from memory.
  // This greatly reduces crackle/stutter caused by flash/LittleFS stalls while WiFi/NeoPixels are active.
  static constexpr size_t RAM_CACHE_MAX_BYTES = 256 * 1024;   // 256 KB

  // Hard cap the requested volume to keep playback comfortable and avoid clipping.
  // If you need it louder later, raise this cap.
  static constexpr uint8_t VOL_HARD_CAP = 40;

  // Audio service task: keeps AudioGeneratorWAV fed even if the main loop is busy.
  static constexpr uint32_t AUDIO_TASK_STACK_WORDS = 4096;    // 4096 words (~16KB)
  static constexpr UBaseType_t AUDIO_TASK_PRIO     = 3;       // higher than Arduino loop task
  static constexpr BaseType_t AUDIO_TASK_CORE      = 1;       // keep off WiFi core

  // --------------------------
  // Audio chain objects
  // --------------------------
  static AudioFileSourceLittleFS* s_file   = nullptr;  // streaming source (flash)
  static AudioFileSourceBuffer*   s_buf    = nullptr;  // streaming buffer (wraps s_src)
  #if PZ_AUDIO_HAVE_PROGMEM_SRC
  static PzMemSrcT*  s_memSrc = nullptr;  // memory source (wraps s_cacheData)
#endif
  static AudioGeneratorWAV*       s_wav    = nullptr;
  static AudioOutputI2S*          s_out    = nullptr;

  static bool     s_loop     = false;
  static uint8_t  s_lastClip = 0;
  static char     s_lastPath[48] = {0};
  static float    s_gain = 0.20f;   // default quieter; 0.0..1.0

  // --------------------------
  // One-clip RAM cache (most recently loaded clip)
  // --------------------------
  static uint8_t* s_cacheData = nullptr;
  static size_t   s_cacheLen  = 0;
  static char     s_cachePath[48] = {0};

#if defined(ARDUINO_ARCH_ESP32)
  static SemaphoreHandle_t s_lock = nullptr;
  static TaskHandle_t      s_task = nullptr;
#endif

  static inline void lockAudio() {
#if defined(ARDUINO_ARCH_ESP32)
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
#endif
  }
  static inline void unlockAudio() {
#if defined(ARDUINO_ARCH_ESP32)
    if (s_lock) xSemaphoreGive(s_lock);
#endif
  }

  // Allocate in PSRAM if present; otherwise fall back to heap.
  static uint8_t* allocAudioMem(size_t n) {
#if defined(ARDUINO_ARCH_ESP32)
    if (psramFound()) {
      uint8_t* p = (uint8_t*)ps_malloc(n);
      if (p) return p;
    }
#endif
    return (uint8_t*)malloc(n);
  }

  static void freeAudioMem(void* p) {
    free(p);
  }

  static void closeChainLocked() {
    if (s_wav) { s_wav->stop(); delete s_wav; s_wav = nullptr; }
    if (s_buf) { delete s_buf;  s_buf  = nullptr; }
    if (s_file){ delete s_file; s_file = nullptr; }
    #if PZ_AUDIO_HAVE_PROGMEM_SRC
    if (s_memSrc){ delete s_memSrc; s_memSrc = nullptr; }
#endif
  }

  static void stopLocked() {
    s_loop = false;
    s_lastClip = 0;
    s_lastPath[0] = '\0';
    closeChainLocked();
  }

  // Ensure s_cacheData holds 'path' (if eligible). Returns true if the cache contains path afterwards.
  static bool ensureCachedLocked(const char* path) {
#if !PZ_AUDIO_HAVE_PROGMEM_SRC
    (void)path;
    return false;
#else
    if (!path || !path[0]) return false;
    if (s_cacheData && (strncmp(path, s_cachePath, sizeof(s_cachePath)) == 0)) return true;

    if (!LittleFS.exists(path)) return false;

    File f = LittleFS.open(path, FILE_READ);
    if (!f) return false;

    size_t sz = (size_t)f.size();
    if (sz == 0 || sz > RAM_CACHE_MAX_BYTES) {
      f.close();
      return false;
    }

    uint8_t* mem = allocAudioMem(sz);
    if (!mem) { f.close(); return false; }

    size_t got = f.read(mem, sz);
    f.close();

    if (got != sz) {
      freeAudioMem(mem);
      return false;
    }

    // Replace previous cache
    if (s_cacheData) {
      freeAudioMem(s_cacheData);
      s_cacheData = nullptr;
      s_cacheLen  = 0;
      s_cachePath[0] = '\0';
    }

    s_cacheData = mem;
    s_cacheLen  = sz;
    strlcpy(s_cachePath, path, sizeof(s_cachePath));
    return true;
  }
#endif

  static bool startPathLocked(const char* path) {
    closeChainLocked();
    if (!path || !path[0]) return false;

    // Prefer RAM preload when possible.
    bool cached = ensureCachedLocked(path);

    AudioFileSource* src = nullptr;

    if (cached && s_cacheData && s_cacheLen) {
#if PZ_AUDIO_HAVE_PROGMEM_SRC
      // AudioFileSourcePROGMEM reads via pgm_read_* helpers, which also works for RAM on ESP32.
      s_memSrc = new PzMemSrcT((const uint8_t*)s_cacheData, (uint32_t)s_cacheLen);
      src = s_memSrc;

      // Even from RAM, a small buffer smooths decode demand and reduces CPU churn.
      s_buf = new AudioFileSourceBuffer(src, 4096);
      src = s_buf;
#else
      // PROGMEM source class not available -> stream instead.
      cached = false;
#endif
    }
    if (!cached) {
      // Fall back to streaming from flash/LittleFS
      s_file = new AudioFileSourceLittleFS(path);
      if (!s_file) return false;
      s_buf  = new AudioFileSourceBuffer(s_file, STREAM_BUFFER_BYTES);
      if (!s_buf) { closeChainLocked(); return false; }
      src = s_buf;
    }

    s_wav = new AudioGeneratorWAV();
    if (!s_wav->begin(src, s_out)) {
      closeChainLocked();
      return false;
    }
    return true;
  }

  static void serviceLoopLocked() {
    if (!s_wav) return;

    if (s_wav->isRunning()) {
      if (!s_wav->loop()) {
        if (s_loop && s_lastPath[0]) {
          // Loop without going back to flash if it's cached
          startPathLocked(s_lastPath);
        } else {
          stopLocked();
        }
      }
    } else {
      if (s_loop && s_lastPath[0]) startPathLocked(s_lastPath);
      else stopLocked();
    }
  }

#if defined(ARDUINO_ARCH_ESP32)
  static void audioTask(void*) {
    for (;;) {
      lockAudio();
      serviceLoopLocked();
      unlockAudio();
      vTaskDelay(1); // yield
    }
  }
#endif

  void begin(int bclkPin, int lrckPin, int doutPin) {
    if (!LittleFS.begin()) LittleFS.begin(true);

#if defined(ARDUINO_ARCH_ESP32)
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
#endif

    lockAudio();
    if (!s_out) {
      // External I2S pins.
      s_out = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S);
      s_out->SetPinout(bclkPin, lrckPin, doutPin);    // (BCLK, LRCK/WS, DOUT)

      // Many mono I2S amps behave best with standard 2-channel frames even for mono content.
      s_out->SetChannels(2);

      s_out->SetGain(s_gain);
    }
    unlockAudio();

#if defined(ARDUINO_ARCH_ESP32)
    // Start a dedicated service task once. This keeps audio stable even while the main loop is busy.
    if (!s_task) {
      xTaskCreatePinnedToCore(audioTask, "pz_audio", AUDIO_TASK_STACK_WORDS,
                              nullptr, AUDIO_TASK_PRIO, &s_task, AUDIO_TASK_CORE);
    }
#endif
  }

  bool playPath(const char* path, bool loop) {
    lockAudio();
    s_loop = loop;
    strlcpy(s_lastPath, path ? path : "", sizeof(s_lastPath));
    s_lastClip = 0;
    bool ok = startPathLocked(path);
    unlockAudio();
    return ok;
  }

  bool playClip(uint8_t clipId, bool loop) {
    char path[32];
    snprintf(path, sizeof(path), "/clips/%03u.wav", (unsigned)clipId);
    lockAudio();
    s_lastClip = clipId;
    s_loop = loop;
    strlcpy(s_lastPath, path, sizeof(s_lastPath));
    bool ok = startPathLocked(path);
    unlockAudio();
    return ok;
  }

  void stop() {
    lockAudio();
    stopLocked();
    unlockAudio();
  }

  void setVolume(uint8_t vol) {
    // The game's server historically used 0..255.
    // On the Feather S3 + MAX98357A this can get VERY loud and may clip.
    // Most installs are comfortable around vol~=10.
    if (vol > VOL_HARD_CAP) vol = VOL_HARD_CAP;

    float g = (float)vol / 255.0f;    // 0..~0.16 with the default cap
    if (g > 1.0f) g = 1.0f;
    s_gain = g;

    lockAudio();
    if (s_out) s_out->SetGain(s_gain);
    unlockAudio();
  }

  bool isPlaying() {
    lockAudio();
    bool r = (s_wav && s_wav->isRunning());
    unlockAudio();
    return r;
  }

  void loop() {
#if defined(ARDUINO_ARCH_ESP32)
    // When the service task is running, loop() is optional and intentionally a no-op.
    if (s_task) return;
#endif
    lockAudio();
    serviceLoopLocked();
    unlockAudio();
  }

} // namespace PizzaAudioFS
