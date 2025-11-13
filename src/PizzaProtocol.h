#pragma once
#include <Arduino.h>

// ===== Protocol version =====
#ifndef PROTOCOL_VERSION
  #define PROTOCOL_VERSION 1
#endif


// Keep text comfortably below ESP-NOW frame limits:
static const uint8_t PZ_ORDERS_MAX      = 6;
static const uint8_t PZ_ORDER_TEXT_MAX  = 120;  // long clue-style orders

// ===== Roles =====
enum Role : uint8_t {
  HOUSE_PANEL, HOUSE_NODE, ORDERS_PANEL, ORDERS_NODE, PIZZA_NODE, CENTRAL
};

// ===== Message types =====
enum MsgType : uint8_t {
  HELLO = 1, HELLO_REQ,
  PANEL_TEXT, SOUND_PLAY,
  DELIVER_SCAN, DELIVER_RESULT,
  OTA_START, OTA_ACK, OTA_RESULT,
  ACK_GENERIC,
  // Device claiming/provisioning (set house_id permanently in NVS)
  CLAIM = 200,
  PIZZA_ING_UPDATE = 210,
  PIZZA_ING_QUERY    = 211,  // Pizza node asks Central for current mask of a UID
  PIZZA_ING_SNAPSHOT = 212,  // Central replies with {uid, mask, ok}
  ORDER_LIST_RESET  = 233,
  ORDER_ITEM_SET    = 234,
  ORDER_SHOW_TEXT   = 235,
  HOUSE_DIGITAL_SET = 240,
  ASSET_SYNC        = 241,
  ASSET_RESULT      = 242,
  NET_CFG_SET       = 250
};

// compact caps for order text on panels
enum HousePanelMode : uint8_t {
  PANEL_MODE_TEXT   = 0,   // use panel_text + style/speed/bright
  PANEL_MODE_NUMBER = 1,   // panel_text carries digits "123"
  PANEL_MODE_LETTER = 2    // panel_text carries single letter "A"
};

enum WindowFx : uint8_t {
  WIN_FX_OFF    = 0,
  WIN_FX_SOLID  = 1,       // HSV + brightness
  WIN_FX_RAINBOW= 2,       // simple moving rainbow
  WIN_FX_PARTY  = 3        // fast random splash
};

enum DeliverReason : uint8_t {
  DR_OK = 0,
  DR_UNKNOWN_PIZZA = 1,
  DR_NO_ORDER = 2,
  DR_WRONG_PIZZA = 3,
};

// ===== Packed header (keep payloads small: <= ~200B total) =====
struct __attribute__((packed)) MsgHeader {
  uint8_t  type;
  uint8_t  role;       // sender role
  uint8_t  house_id;   // 0 if N/A
  uint16_t seq;        // per-sender
  uint16_t len;        // payload bytes
  uint16_t crc16;      // header (with crc16=0) + payload
};

// ===== Payload sketches (keep short, deterministic) =====
struct HelloPayload {
  char     fw[12];     // "0.1.0"
  uint8_t  proto;      // PROTOCOL_VERSION
  uint8_t  mac[6];
};

struct PanelTextPayload {
  uint8_t  house_id;
  char     text[96];
  uint8_t  style;      // 0=scroll,1=static,2=wipe,3=border
  uint8_t  speed;      // 1..5
  uint8_t  bright;     // 0..255
};

struct SoundPlayPayload {
  uint8_t house_id;
  uint8_t clip_id;
  uint8_t vol;         // 0..255
};

struct DeliverScanPayload {
  uint8_t  house_id;
  uint8_t  uid[10];
  uint8_t  uid_len;    // 4..7 typical for MIFARE
};

struct DeliverResultPayload {
  uint8_t ok;          // 1=OK, 0=ERR
  uint8_t reason;      // 0=OK, 1=DR_UNKNOWN_PIZZA, 2=DR_NO_ORDER, 3=DR_WRONG_PIZZA
};

struct OtaStartPayload {
  uint8_t target_role; // Role
  uint8_t scope;       // 0=ALL, 1=LIST
  uint8_t ids[8];      // house ids if scope=LIST (unused filled with 0)
  char    url[160];     // absolute URL (http://host:port/...bin)
  char    ver[12];
};

struct ClaimPayload {
  uint8_t target_mac[6];  // device to claim (STA MAC)
  uint8_t house_id;       // new house id (1..6)
  uint8_t force;          // 0=no (only if unclaimed), 1=yes (overwrite)
};

struct PizzaIngrUpdatePayload {
  uint8_t uid[10];
  uint8_t uid_len;   // 4..7 typical
  uint8_t mask;      // bit0..bit4 = pepperoni,mushrooms,peppers,pineapple,ham
};

// Add payloads near PizzaIngrUpdatePayload:
struct PizzaIngrQueryPayload {
  uint8_t uid[10];
  uint8_t uid_len;   // 4..7 typical
};

struct PizzaIngrSnapshotPayload {
  uint8_t uid[10];
  uint8_t uid_len;
  uint8_t mask;      // bit0..bit4 = P, M, Pe, Pi, H
  uint8_t ok;        // 1=found, 0=unknown (treat as 0 mask)
};

struct PzOrderListResetPayload {
  uint8_t count; // 0..PZ_ORDERS_MAX
};

struct PzOrderItemSetPayload {
  uint8_t index;     // 0..count-1
  uint8_t house_id;  // 1..6 (or 0 if N/A)
  uint8_t mask;      // toppings bitmask (0..31) – for future logic
  char    text[PZ_ORDER_TEXT_MAX]; // NUL-terminated or NUL-padded
};

struct PzOrderShowTextPayload {
  char text[PZ_ORDER_TEXT_MAX];    // NUL-terminated or NUL-padded
};

struct OtaAckPayload { uint8_t accept; uint8_t code; };    // 1/0
struct OtaResultPayload { uint8_t ok; uint8_t code; };     // 1/0

// One-shot "describe everything" for a House
struct HouseDigitalSetPayload {
  uint8_t  house_id;       // target
  uint8_t  flags;          // b0=window, b1=panel, b2=speaker

  // window
  uint8_t  win_fx;         // WindowFx
  uint8_t  win_h, win_s, win_v;
  uint8_t  win_speed;      // 0..255 (effect-dependent)

  // panel
  uint8_t  panel_mode;     // HousePanelMode
  char     panel_text[24]; // small & fast
  uint8_t  panel_style;    // 0..n (same as PANEL_TEXT)
  uint8_t  panel_speed;    // 1..5
  uint8_t  panel_bright;   // 0..255

  // speaker
  uint8_t  spk_clip;       // 1..N -> "/clips/NNN.wav"
  uint8_t  spk_vol;        // 0..255
  uint8_t  spk_flags;      // b0=loop, b1=stop (stop wins)
};

// Tell a HouseNode to fetch clip files into /clips
struct AssetSyncPayload {
  uint8_t  house_id;          // target node
  char     base_url[96];      // e.g. "http://10.0.0.5/pizza/h3"
  uint8_t  count;             // how many clips to fetch: 1..30
};

// Optional result message (progress/OK)
struct AssetResultPayload {
  uint8_t  house_id;
  uint8_t  ok;                // 1=ok, 0=err
  uint8_t  count_done;        // fetched successfully
  uint8_t  code;              // 0=ok, else small error codes
};

// SSID/PASS/BASE distribution payload
struct NetCfgSetPayload {
  char ssid[32];   // NUL-terminated
  char pass[64];   // NUL-terminated
  char base[96];   // NUL-terminated (asset/OTA base URL)  <-- was 128
};

// ===== Helpers =====
namespace PizzaProtocol {
  // CRC16-CCITT (0x1021, init=0xFFFF)
  uint16_t crc16(const uint8_t* data, size_t len);

  // Pack header+payload into outBuf; returns total bytes, 0 on error
  size_t pack(uint8_t type, Role role, uint8_t house_id,
              uint16_t seq, const void* payload, uint16_t payload_len,
              uint8_t* outBuf, uint16_t outMax);

  // Unpack header; returns true if CRC ok and lengths valid
  bool unpack(const uint8_t* inBuf, uint16_t inLen,
              MsgHeader& outHdr, const uint8_t*& outPayload, uint16_t& outPayLen);
}
