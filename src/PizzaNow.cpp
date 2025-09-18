#include "PizzaNow.h"
#include <esp_idf_version.h>

// Keep our handler + init flag
static PizzaNow::RxHandler s_rx;
static bool s_inited = false;

// Broadcast MAC
static uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
// ===== IDF 5.x callback signatures =====
static void onRecvCB(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (!s_rx || len < (int)sizeof(MsgHeader)) return;
  MsgHeader hdr; const uint8_t* payload; uint16_t plen;
  if (!PizzaProtocol::unpack(data, len, hdr, payload, plen)) return;
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
  if (!s_rx || len < (int)sizeof(MsgHeader)) return;
  MsgHeader hdr; const uint8_t* payload; uint16_t plen;
  if (!PizzaProtocol::unpack(data, len, hdr, payload, plen)) return;
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
  WiFi.disconnect(true, true);
  delay(50);

  // Lock radio to our ESPNOW runtime channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

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
  esp_now_add_peer(&peer);

  s_inited = true;
  PZ_LOGI("ESPNOW init on ch %u OK", channel);
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
  return esp_now_send(BROADCAST_MAC, data, len) == ESP_OK;
}

bool addPeer(const uint8_t mac[6]) {
  if (!s_inited) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx   = WIFI_IF_STA;
  peer.channel = ESPNOW_CHANNEL;
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
  return esp_now_send(mac, data, len) == ESP_OK;
}

void onReceive(RxHandler cb) { s_rx = cb; }

} // namespace PizzaNow
