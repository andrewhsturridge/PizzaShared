#pragma once
#include <Arduino.h>

namespace PizzaOta {
  enum Result : uint8_t { OK=0, WIFI_FAIL=1, HTTP_FAIL=2, SIZE_ZERO=3, UPDATE_FAIL=4, TIMEOUT=5 };

  // Progress callback: total==0 means unknown size. Called from loop context.
  typedef void (*ProgressCB)(size_t written, size_t total);
  void setProgressCallback(ProgressCB cb);
  bool beginWifi(uint32_t timeoutMs);   // true when WL_CONNECTED
  void endWifi();                       // clean Wi-Fi disconnect

  // Performs full OTA pull (blocking in loop context):
  // 1) PizzaNow::deinit()
  // 2) Wi-Fi STA connect (WIFI_SSID/PASS)
  // 3) HTTP GET .bin and Update (streams with progress callbacks)
  // 4) On success, shows 100% via progress callback and reboots
  Result start(const char* absoluteUrl, const char* newVersion, uint32_t totalTimeoutMs = 60000);
}