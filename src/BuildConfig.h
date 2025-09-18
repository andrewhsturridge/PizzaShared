#pragma once

// --- Versions ---
#define PROTOCOL_VERSION   1
#define FW_VERSION         "0.1.0"

// --- Radio (runtime) ---
#define ESPNOW_CHANNEL     6   // fixed; Wi-Fi STA only during OTA window

// --- OTA Wi-Fi (updates only) ---
#define WIFI_SSID          "GUD"
#define WIFI_PASS          "EscapE66"

// --- OTA base host (your dev box running: python3 -m http.server 8000) ---
#define OTA_BASE_URL       "http://192.168.2.231:8000/"

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
#define OTA_REL_ORDERS_NODE   "PizzaDelivery/Pizza_OrdersNode/build/esp32.esp32.esp32/Pizza_OrdersNode.ino.bin"
#define OTA_REL_PIZZA_NODE    "PizzaDelivery/Pizza_PizzaNode/build/esp32.esp32.esp32/Pizza_PizzaNode.ino.bin"

// --- Timeouts / retries ---
#define ACK_TIMEOUT_MS      60
#define ACK_RETRIES         2
#define HELLO_BACKOFF_MS    500
#define OTA_TOTAL_MS        60000
