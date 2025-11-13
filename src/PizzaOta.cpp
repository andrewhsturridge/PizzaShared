#include "PizzaOta.h"
#include "BuildConfig.h"
#include "PizzaNow.h"
#include "PizzaUtils.h"
#include "PizzaNetCfg.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_wifi.h>

namespace PizzaOta {

static ProgressCB s_cb = nullptr;
void setProgressCallback(ProgressCB cb){ s_cb = cb; }

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
  NetCfg::Value net{};
  NetCfg::load(net);

  // 1) Ensure clean STA state
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);   // drop and clear creds
  delay(50);

  // 2) Make sure driver is up and not in PS
  esp_wifi_stop();               // stop if running (ok if already stopped)
  esp_wifi_start();              // start fresh
  esp_wifi_set_ps(WIFI_PS_NONE);

  // 3) Begin once with runtime creds
  PZ_LOGI("WiFi: begin ssid=\"%s\"", net.ssid);
  WiFi.begin(net.ssid, net.pass);

  // 4) Wait for CONNECTED or timeout, logging transitions
  uint32_t t0 = millis();
  wl_status_t last = (wl_status_t)255;
  while (true) {
    wl_status_t st = WiFi.status();
    if (st != last) { last = st; PZ_LOGI("WiFi: status=%s", wlName(st)); }
    if (st == WL_CONNECTED) {
      PZ_LOGI("WiFi: IP %s ch=%d RSSI=%d",
        WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
      return true;
    }
    if ((millis() - t0) > timeoutMs) {
      PZ_LOGE("WiFi: connect timeout (%s)", wlName(st));
      return false;
    }
    delay(100);
  }
}

Result start(const char* url, const char* newVersion, uint32_t totalTimeoutMs) {
  (void)newVersion;
  PZ_LOGI("OTA start: %s", url);

  PizzaNow::deinit();
  delay(50);

  uint32_t wifiBudget = totalTimeoutMs > 30000 ? totalTimeoutMs/3 : totalTimeoutMs/2;
  if (!wifiConnect(wifiBudget)) {
    WiFi.disconnect(true, true);
    return WIFI_FAIL;
  }

  WiFiClient client; client.setTimeout(20000);

  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true); // avoid chunked edge cases

  if (!http.begin(client, url)) {
    PZ_LOGE("OTA: HTTP begin failed");
    http.end(); WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  int code = http.GET();
  PZ_LOGI("OTA: HTTP GET -> %d", code);
  if (code != HTTP_CODE_OK) {
    http.end(); WiFi.disconnect(true, true);
    return HTTP_FAIL;
  }

  int len = http.getSize();
  PZ_LOGI("OTA: content length = %d", len);
  size_t totalLen = (len > 0) ? (size_t)len : 0;

  if (totalLen == 0) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      PZ_LOGE("OTA: Update.begin(unknown) failed");
      http.end(); WiFi.disconnect(true, true);
      return UPDATE_FAIL;
    }
  } else {
    if (!Update.begin(totalLen)) {
      PZ_LOGE("OTA: Update.begin(%u) failed", (unsigned)totalLen);
      http.end(); WiFi.disconnect(true, true);
      return UPDATE_FAIL;
    }
  }

  if (s_cb) s_cb(0, totalLen); // show 0% (or unknown)

  WiFiClient& stream = http.getStream();
  uint8_t buf[2048];
  size_t totalWritten = 0;
  uint32_t lastProgress = millis();
  uint32_t startAt = lastProgress;

  while (http.connected() && (len > 0 || totalLen == 0)) {
    size_t n = stream.readBytes(buf, sizeof(buf));
    if (n > 0) {
      if (Update.write(buf, n) != n) {
        PZ_LOGE("OTA: Update.write failed at %u bytes", (unsigned)totalWritten);
        Update.abort(); http.end(); WiFi.disconnect(true, true);
        return UPDATE_FAIL;
      }
      totalWritten += n;

      // notify progress each chunk or ~200ms
      if (s_cb && (millis() - lastProgress > 200)) {
        s_cb(totalWritten, totalLen);
        lastProgress = millis();
      }

      if (len > 0) len -= n;
    } else {
      if (millis() - lastProgress > 1500) {
        PZ_LOGI("OTA: waiting for data...");
        lastProgress = millis();
        if (s_cb) s_cb(totalWritten, totalLen);
      }
    }

    if (millis() - startAt > totalTimeoutMs) {
      PZ_LOGE("OTA: overall timeout after %u bytes", (unsigned)totalWritten);
      Update.abort(); http.end(); WiFi.disconnect(true, true);
      return TIMEOUT;
    }
  }

  if (!Update.end(true)) {
    PZ_LOGE("OTA: Update.end failed");
    http.end(); WiFi.disconnect(true, true);
    return UPDATE_FAIL;
  }
  
  // Force a final "done" notification so the panel can draw DONE
  if (s_cb) s_cb(1, 1);

  http.end();
  WiFi.disconnect(true, true);
  PZ_LOGI("OTA: OK, rebooting");
  delay(OTA_DONE_HOLD_MS); // let visuals show 100%
  ESP.restart();
  return OK; // not reached
}

bool beginWifi(uint32_t timeoutMs){
  // Reuse the exact OTA path (esp_wifi_start + WIFI_PS_NONE + status loop)
  return wifiConnect(timeoutMs);
}

void endWifi(){
  WiFi.disconnect(true, true);
}

} // namespace
