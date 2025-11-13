#pragma once

// --- Versions ---
#define PROTOCOL_VERSION   1
#define FW_VERSION         "0.1.0"

// --- Radio (runtime) ---
#define ESPNOW_CHANNEL     6   // fixed; Wi-Fi STA only during OTA window

// --- Network defaults (used by NetCfg compiled defaults) ---
#ifndef WIFI_DEFAULT_SSID
  #define WIFI_DEFAULT_SSID       "AndrewiPhone"
#endif
#ifndef WIFI_DEFAULT_PASS
  #define WIFI_DEFAULT_PASS       "12345678"
#endif
#ifndef OTA_BASE_URL_DEFAULT
  // Example: your MacBook’s simple HTTP host for assets/firmware
  #define OTA_BASE_URL_DEFAULT    "http://172.20.10.2:8000/"
#endif

// ---- Back-compat aliases (optional) ----
// If any old code still references these, it will now pick up the new defaults.
#ifndef WIFI_SSID
  #define WIFI_SSID               WIFI_DEFAULT_SSID
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS               WIFI_DEFAULT_PASS
#endif
#ifndef OTA_BASE_URL
  #define OTA_BASE_URL            OTA_BASE_URL_DEFAULT
#endif

// --- OTA / Wi-Fi tunables ---
#ifndef OTA_WIFI_CONNECT_MS
  #define OTA_WIFI_CONNECT_MS   45000   // per-attempt connect budget (45s)
#endif
#ifndef OTA_WIFI_RETRIES
  #define OTA_WIFI_RETRIES      4       // total attempts
#endif
#ifndef OTA_RETRY_BACKOFF_MS
  #define OTA_RETRY_BACKOFF_MS  3000    // gap between attempts
#endif
#ifndef OTA_HTTP_CONNECT_MS
  #define OTA_HTTP_CONNECT_MS   20000   // HTTP connect timeout
#endif
#ifndef OTA_HTTP_TOTAL_MS
  #define OTA_HTTP_TOTAL_MS     60000   // HTTP total read timeout
#endif

// --- Role-relative .bin paths (Arduino "Export compiled binary" output) ---
// NOTE: Keep these in sync with your sketch folder names & selected boards.
// MatrixPortal S3 build folder ID (from you): esp32.esp32.adafruit_matrixportal_esp32s3
#define OTA_REL_HOUSE_PANEL   "PizzaDelivery/Pizza_HousePanel/build/esp32.esp32.adafruit_matrixportal_esp32s3/Pizza_HousePanel.ino.bin"
#define OTA_REL_ORDERS_PANEL  "PizzaDelivery/Pizza_OrdersPanel/build/esp32.esp32.adafruit_matrixportal_esp32s3/Pizza_OrdersPanel.ino.bin"

// FeatherS3 board ID (usually "esp32.esp32.um_feathers3")
#define OTA_REL_HOUSE_NODE    "PizzaDelivery/Pizza_HouseNode/build/esp32.esp32.um_feathers3/Pizza_HouseNode.ino.bin"
#define OTA_REL_CENTRAL       "PizzaDelivery/Pizza_Central/build/esp32.esp32.um_feathers3/Pizza_Central.ino.bin"

// Classic ESP32 Dev Module (Arduino board id: esp32.esp32.esp32). If you pick a different board,
// the folder name will change—update these two defines accordingly.
#define OTA_REL_ORDERS_NODE   "PizzaDelivery/Pizza_OrderStation/build/esp32.esp32.esp32/Pizza_OrderStation.ino.bin"
#define OTA_REL_PIZZA_NODE    "PizzaDelivery/Pizza_PizzaNode/build/esp32.esp32.esp32/Pizza_PizzaNode.ino.bin"

// --- Timeouts / retries ---
#define ACK_TIMEOUT_MS      60
#define ACK_RETRIES         2
#define HELLO_BACKOFF_MS    500
#define OTA_TOTAL_MS        120000

#ifndef OTA_DONE_HOLD_MS
#define OTA_DONE_HOLD_MS 1000   // 1 second hold before ESP.restart()
#endif

