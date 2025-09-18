#pragma once
#include <Arduino.h>
#include "PizzaProtocol.h"
#include "PizzaUtils.h"
#include "BuildConfig.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

namespace PizzaNow {
  typedef std::function<void(const MsgHeader&, const uint8_t* payload, uint16_t len, const uint8_t srcMac[6])> RxHandler;

  bool begin(uint8_t channel = ESPNOW_CHANNEL);     // STA mode, ESPNOW init, set channel
  void deinit();                                    // tear down ESPNOW (for OTA window)
  void loop();                                      // placeholder (future retries/timeouts)

  bool sendBroadcast(const uint8_t* data, uint16_t len);
  bool addPeer(const uint8_t mac[6]);
  bool removePeer(const uint8_t mac[6]);
  bool sendUnicast(const uint8_t mac[6], const uint8_t* data, uint16_t len);

  void onReceive(RxHandler cb);
}
