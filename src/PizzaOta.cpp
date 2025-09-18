#include "PizzaOta.h"
#include "BuildConfig.h"
#include "PizzaNow.h"
#include "PizzaUtils.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

namespace PizzaOta {

static bool wifiConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if ((millis() - t0) > timeoutMs) return false;
    delay(50);
  }
  return true;
}

Result start(const char* url, const char* newVersion, uint32_t totalTimeoutMs) {
  (void)newVersion;
  PZ_LOGI("OTA start: %s", url);

  PizzaNow::deinit();

  if (!wifiConnect(totalTimeoutMs/3)) {
    PZ_LOGE("WiFi connect failed");
    return WIFI_FAIL;
  }

  HTTPClient http;
  if (!http.begin(url)) {
    PZ_LOGE("HTTP begin failed");
    WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    PZ_LOGE("HTTP GET failed: %d", code);
    http.end();
    WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  int len = http.getSize();
  if (len <= 0) {
    PZ_LOGE("HTTP size zero");
    http.end();
    WiFi.disconnect(true, true);
    return SIZE_ZERO;
  }

  WiFiClient& stream = http.getStream();
  if (!Update.begin(len)) {
    PZ_LOGE("Update.begin failed");
    http.end();
    WiFi.disconnect(true, true);
    return UPDATE_FAIL;
  }

  uint8_t buf[1024];
  uint32_t t0 = millis();
  while (http.connected() && (len > 0 || len == -1)) {
    size_t n = stream.readBytes(buf, sizeof(buf));
    if (n > 0) {
      if (Update.write(buf, n) != n) {
        PZ_LOGE("Update.write failed");
        Update.abort();
        http.end();
        WiFi.disconnect(true, true);
        return UPDATE_FAIL;
      }
      t0 = millis();
    } else {
      if ((millis() - t0) > totalTimeoutMs) {
        PZ_LOGE("OTA timeout");
        Update.abort();
        http.end();
        WiFi.disconnect(true, true);
        return TIMEOUT;
      }
      delay(10);
    }
  }

  if (!Update.end(true)) {
    PZ_LOGE("Update.end failed");
    http.end();
    WiFi.disconnect(true, true);
    return UPDATE_FAIL;
  }

  http.end();
  WiFi.disconnect(true, true);
  PZ_LOGI("OTA OK, rebooting...");
  delay(100);
  ESP.restart();
  return OK; // unreachable
}

} // namespace
