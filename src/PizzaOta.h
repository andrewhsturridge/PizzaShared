#pragma once
#include <Arduino.h>

namespace PizzaOta {
  enum Result : uint8_t { OK=0, WIFI_FAIL=1, HTTP_FAIL=2, SIZE_ZERO=3, UPDATE_FAIL=4, TIMEOUT=5 };

  // Performs full OTA pull:
  // 1) PizzaNow::deinit()
  // 2) Wi-Fi STA connect (WIFI_SSID/PASS)
  // 3) HTTP GET .bin and Update
  // 4) return result; caller may reboot on OK
  Result start(const char* absoluteUrl, const char* newVersion, uint32_t totalTimeoutMs = 60000);
}
