#include "PizzaNow.h"
#include <esp_idf_version.h>
#include <esp_err.h>

// Keep our handler + init flag
static PizzaNow::RxHandler s_rx;
static bool s_inited = false;
static uint8_t s_channel = ESPNOW_CHANNEL;

// Broadcast MAC
static uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Diagnostics: when strict framing is enabled, it can be confusing if some
// devices are still transmitting legacy (unframed) packets. We print a small
// number of "dropped non-framed" logs to make mismatches obvious.
static uint8_t s_diagDropNonFramed = 0;
static uint8_t s_diagSendErr = 0;

// ===== Wire framing =====
// We keep this in the ESPNOW layer so application code can continue using
// PizzaProtocol::pack/unpack unchanged.
static inline bool stripWire(const uint8_t* in, int inLen, const uint8_t*& out, int& outLen) {
  if (!in || inLen <= 0) return false;

  // New framed packets
  if (inLen >= 3 &&
      in[0] == (uint8_t)PZ_WIRE_MAGIC0 &&
      in[1] == (uint8_t)PZ_WIRE_MAGIC1 &&
      in[2] == (uint8_t)PZ_WIRE_VERSION) {
    out = in + 3;
    outLen = inLen - 3;
    return true;
  }

#if PZ_WIRE_RX_LEGACY
  // Legacy packets (no wire prefix)
  out = in;
  outLen = inLen;
  return true;
#else
  return false;
#endif
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
// ===== IDF 5.x callback signatures =====
static void onRecvCB(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (!s_rx) return;

  const uint8_t* inner = nullptr;
  int innerLen = 0;
  if (!stripWire(data, len, inner, innerLen)) {
#if !PZ_WIRE_RX_LEGACY
    if (s_diagDropNonFramed < 5 && data && len >= 3) {
      PZ_LOGW("Dropped non-framed ESPNOW len=%d first=%02X %02X %02X",
              len, data[0], data[1], data[2]);
      s_diagDropNonFramed++;
    }
#endif
    return;
  }
  if (innerLen < (int)sizeof(MsgHeader)) return;

  MsgHeader hdr; const uint8_t* payload; uint16_t plen;
  if (!PizzaProtocol::unpack(inner, innerLen, hdr, payload, plen)) return;
  const uint8_t* mac = info ? info->src_addr : nullptr;
  uint8_t dummy[6] = {0,0,0,0,0,0};
  s_rx(hdr, payload, plen, mac ? mac : dummy);
}
static void onSendCB(const wifi_tx_info_t* /*info*/, esp_now_send_status_t /*status*/) {
  // hook for future ACK/retry if you want tx info
}
#else
// ===== Legacy IDF 4.x callback signatures (for older cores) =====
static void onRecvCB(const uint8_t* mac, const uint8_t* data, int len) {
  if (!s_rx) return;

  const uint8_t* inner = nullptr;
  int innerLen = 0;
  if (!stripWire(data, len, inner, innerLen)) {
#if !PZ_WIRE_RX_LEGACY
    if (s_diagDropNonFramed < 5 && data && len >= 3) {
      PZ_LOGW("Dropped non-framed ESPNOW len=%d first=%02X %02X %02X",
              len, data[0], data[1], data[2]);
      s_diagDropNonFramed++;
    }
#endif
    return;
  }
  if (innerLen < (int)sizeof(MsgHeader)) return;

  MsgHeader hdr; const uint8_t* payload; uint16_t plen;
  if (!PizzaProtocol::unpack(inner, innerLen, hdr, payload, plen)) return;
  uint8_t dummy[6] = {0,0,0,0,0,0};
  s_rx(hdr, payload, plen, mac ? mac : dummy);
}
static void onSendCB(const uint8_t* /*mac*/, esp_now_send_status_t /*status*/) {
}
#endif

namespace PizzaNow {

bool begin(uint8_t channel) {
  if (s_inited) return true;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(50);

  // Lock radio to our ESPNOW runtime channel
  esp_wifi_start();
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  s_channel = channel;

  if (esp_now_init() != ESP_OK) { PZ_LOGE("esp_now_init failed"); return false; }

  // Register IDF-appropriate callbacks
  esp_now_register_recv_cb(onRecvCB);
  esp_now_register_send_cb(onSendCB);

  // Add broadcast peer once
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.ifidx   = WIFI_IF_STA;
  peer.channel = channel;
  peer.encrypt = false;
  {
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
      PZ_LOGE("add broadcast peer failed: %d", (int)err);
    }
  }

  s_inited = true;
  PZ_LOGI("ESPNOW init on ch %u OK", channel);
  PZ_LOGI("WIRE txFramed=%u rxLegacy=%u magic=%c%c ver=%u",
          (unsigned)PZ_WIRE_TX_FRAMED,
          (unsigned)PZ_WIRE_RX_LEGACY,
          (char)PZ_WIRE_MAGIC0,
          (char)PZ_WIRE_MAGIC1,
          (unsigned)PZ_WIRE_VERSION);
  return true;
}

void deinit() {
  if (!s_inited) return;
  esp_now_deinit();
  s_inited = false;
}

void loop() {
  // Future: retries/ACK mgmt
}

bool sendBroadcast(const uint8_t* data, uint16_t len) {
  if (!s_inited) return false;

#if PZ_WIRE_TX_FRAMED
  uint8_t tmp[256];
  if (!data) return false;
  if ((size_t)len + 3 > sizeof(tmp)) return false;
  tmp[0] = (uint8_t)PZ_WIRE_MAGIC0;
  tmp[1] = (uint8_t)PZ_WIRE_MAGIC1;
  tmp[2] = (uint8_t)PZ_WIRE_VERSION;
  memcpy(tmp + 3, data, len);
  esp_err_t err = esp_now_send(BROADCAST_MAC, tmp, len + 3);
  if (err != ESP_OK && s_diagSendErr < 5) {
    PZ_LOGE("esp_now_send(BCAST) failed: %d", (int)err);
    s_diagSendErr++;
  }
  return err == ESP_OK;
#else
  esp_err_t err = esp_now_send(BROADCAST_MAC, data, len);
  if (err != ESP_OK && s_diagSendErr < 5) {
    PZ_LOGE("esp_now_send(BCAST) failed: %d", (int)err);
    s_diagSendErr++;
  }
  return err == ESP_OK;
#endif
}

bool addPeer(const uint8_t mac[6]) {
  if (!s_inited) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx   = WIFI_IF_STA;
  peer.channel = s_channel;
  peer.encrypt = false;
  esp_now_del_peer(mac); // idempotent
  return esp_now_add_peer(&peer) == ESP_OK;
}

bool removePeer(const uint8_t mac[6]) {
  if (!s_inited) return false;
  return esp_now_del_peer(mac) == ESP_OK;
}

bool sendUnicast(const uint8_t mac[6], const uint8_t* data, uint16_t len) {
  if (!s_inited) return false;

#if PZ_WIRE_TX_FRAMED
  uint8_t tmp[256];
  if (!data) return false;
  if ((size_t)len + 3 > sizeof(tmp)) return false;
  tmp[0] = (uint8_t)PZ_WIRE_MAGIC0;
  tmp[1] = (uint8_t)PZ_WIRE_MAGIC1;
  tmp[2] = (uint8_t)PZ_WIRE_VERSION;
  memcpy(tmp + 3, data, len);
  esp_err_t err = esp_now_send(mac, tmp, len + 3);
  if (err != ESP_OK && s_diagSendErr < 5) {
    PZ_LOGE("esp_now_send(UNI) failed: %d", (int)err);
    s_diagSendErr++;
  }
  return err == ESP_OK;
#else
  esp_err_t err = esp_now_send(mac, data, len);
  if (err != ESP_OK && s_diagSendErr < 5) {
    PZ_LOGE("esp_now_send(UNI) failed: %d", (int)err);
    s_diagSendErr++;
  }
  return err == ESP_OK;
#endif
}

void onReceive(RxHandler cb) { s_rx = cb; }

} // namespace PizzaNow
