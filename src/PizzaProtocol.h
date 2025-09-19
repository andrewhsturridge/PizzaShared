#pragma once
#include <Arduino.h>

// ===== Protocol version =====
#ifndef PROTOCOL_VERSION
  #define PROTOCOL_VERSION 1
#endif

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
  CLAIM = 200
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
  uint8_t reason;      // 0=OK,1=wrong_house,2=unknown_tag,3=expired
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

struct OtaAckPayload { uint8_t accept; uint8_t code; };    // 1/0
struct OtaResultPayload { uint8_t ok; uint8_t code; };     // 1/0

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
