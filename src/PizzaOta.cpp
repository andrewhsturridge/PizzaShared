#include "PizzaOta.h"
#include "BuildConfig.h"
#include "PizzaNow.h"
#include "PizzaUtils.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_wifi.h>

namespace PizzaOta {

static const char* wlName(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:   return "IDLE";
    case WL_SCAN_COMPLETED:return "SCAN_DONE";
    case WL_NO_SSID_AVAIL: return "NO_SSID";
    case WL_CONNECT_FAILED:return "CONNECT_FAIL";
    case WL_CONNECTION_LOST:return "CONN_LOST";
    case WL_DISCONNECTED:  return "DISCONNECTED";
    case WL_CONNECTED:     return "CONNECTED";
    default:               return "UNKNOWN";
  }
}

static bool wifiConnect(uint32_t timeoutMs) {
  PZ_LOGI("WiFi: connecting to SSID=%s", WIFI_SSID);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(50);

  // Make sure driver is running and not sleeping
  esp_wifi_start();                      // safe if already started
  esp_wifi_set_ps(WIFI_PS_NONE);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  wl_status_t last = (wl_status_t)255;
  while (true) {
    wl_status_t st = WiFi.status();
    if (st != last) { last = st; PZ_LOGI("WiFi: status=%s", wlName(st)); }
    if (st == WL_CONNECTED) {
      PZ_LOGI("WiFi: got IP %s  ch=%d  RSSI=%d",
              WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
      return true;
    }
    if ((millis() - t0) > timeoutMs) {
      PZ_LOGE("WiFi: connect timeout (status=%s)", wlName(st));
      return false;
    }
    delay(100);
  }
}

Result start(const char* url, const char* newVersion, uint32_t totalTimeoutMs) {
  (void)newVersion;
  PZ_LOGI("OTA start: %s", url);

  // 1) Quiesce radio transport
  PizzaNow::deinit();    // stop ESPNOW; leaves Wi-Fi driver up
  delay(50);

  // 2) Connect to AP
  uint32_t wifiBudget = totalTimeoutMs > 30000 ? totalTimeoutMs/3 : totalTimeoutMs/2;
  if (!wifiConnect(wifiBudget)) {
    PZ_LOGE("OTA: WiFi connect failed");
    WiFi.disconnect(true, true);
    return WIFI_FAIL;
  }

  // 3) Prepare HTTP client (explicit client instance, longer timeout)
  WiFiClient client;
  client.setTimeout(20000);        // 20 s socket timeout

  HTTPClient http;
  http.setConnectTimeout(15000);   // 15 s connect timeout
  http.setTimeout(20000);          // 20 s header/body timeout
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);            // avoid chunked edge cases

  if (!http.begin(client, url)) {
    PZ_LOGE("OTA: HTTP begin failed");
    http.end();
    WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  // 4) GET
  int code = http.GET();
  PZ_LOGI("OTA: HTTP GET -> %d", code);
  if (code != HTTP_CODE_OK) {
    http.end();
    WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  // 5) Size & begin Update
  int len = http.getSize();
  PZ_LOGI("OTA: content length = %d", len);

  bool unknown = (len <= 0);
  if (unknown) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      PZ_LOGE("OTA: Update.begin(unknown) failed");
      http.end(); WiFi.disconnect(true, true);
      return UPDATE_FAIL;
    }
  } else {
    if (!Update.begin(len)) {
      PZ_LOGE("OTA: Update.begin(%d) failed", len);
      http.end(); WiFi.disconnect(true, true);
      return UPDATE_FAIL;
    }
  }

  // 6) Stream to flash (progress logs)
  WiFiClient& stream = http.getStream();
  uint8_t buf[2048];
  size_t totalWritten = 0;
  uint32_t lastLog = millis();
  uint32_t startAt = lastLog;

  while (http.connected() && (len > 0 || unknown)) {
    size_t n = stream.readBytes(buf, sizeof(buf));
    if (n > 0) {
      if (Update.write(buf, n) != n) {
        PZ_LOGE("OTA: Update.write failed at %u bytes", (unsigned)totalWritten);
        Update.abort(); http.end(); WiFi.disconnect(true, true);
        return UPDATE_FAIL;
      }
      totalWritten += n;
      if (!unknown && len > 0) {
        // log every ~10% or 2s
        if (millis() - lastLog > 2000) {
          int pct = (int)((totalWritten * 100ULL) / (size_t)len);
          PZ_LOGI("OTA: %u/%d bytes (%d%%)", (unsigned)totalWritten, len, pct);
          lastLog = millis();
        }
      } else {
        if (millis() - lastLog > 2000) {
          PZ_LOGI("OTA: %u bytes...", (unsigned)totalWritten);
          lastLog = millis();
        }
      }
      if (!unknown) len -= n;
    } else {
      if (millis() - lastLog > 5000) {
        PZ_LOGI("OTA: waiting for data...");
        lastLog = millis();
      }
    }

    // global timeout
    if (millis() - startAt > totalTimeoutMs) {
      PZ_LOGE("OTA: overall timeout after %u bytes", (unsigned)totalWritten);
      Update.abort(); http.end(); WiFi.disconnect(true, true);
      return TIMEOUT;
    }
  }

  // 7) Finish & reboot
  if (!Update.end(true)) {
    PZ_LOGE("OTA: Update.end failed");
    http.end(); WiFi.disconnect(true, true);
    return UPDATE_FAIL;
  }

  http.end();
  WiFi.disconnect(true, true);
  PZ_LOGI("OTA: OK, rebooting");
  delay(100);
  ESP.restart();
  return OK; // not reached
}

} // namespace
