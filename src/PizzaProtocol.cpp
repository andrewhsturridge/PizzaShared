#include "PizzaProtocol.h"

namespace PizzaProtocol {

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i=0;i<8;i++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else              crc = (crc << 1);
    }
  }
  return crc;
}

size_t pack(uint8_t type, Role role, uint8_t house_id,
            uint16_t seq, const void* payload, uint16_t payload_len,
            uint8_t* outBuf, uint16_t outMax) {
  if (!outBuf) return 0;
  if (payload_len > 200) return 0;                // keep ESPNOW happy
  if (outMax < sizeof(MsgHeader) + payload_len) return 0;

  MsgHeader hdr;
  hdr.type     = type;
  hdr.role     = (uint8_t)role;
  hdr.house_id = house_id;
  hdr.seq      = seq;
  hdr.len      = payload_len;
  hdr.crc16    = 0;

  // copy header+payload
  memcpy(outBuf, &hdr, sizeof(hdr));
  if (payload && payload_len) {
    memcpy(outBuf + sizeof(hdr), payload, payload_len);
  }

  // compute CRC over header (with crc16=0) + payload
  uint16_t crc = crc16(outBuf, sizeof(hdr) + payload_len);
  // write crc into buffer
  ((MsgHeader*)outBuf)->crc16 = crc;
  return sizeof(hdr) + payload_len;
}

bool unpack(const uint8_t* inBuf, uint16_t inLen,
            MsgHeader& outHdr, const uint8_t*& outPayload, uint16_t& outPayLen) {
  if (!inBuf || inLen < sizeof(MsgHeader)) return false;

  memcpy(&outHdr, inBuf, sizeof(MsgHeader));
  if (inLen != sizeof(MsgHeader) + outHdr.len) return false;

  // Recompute CRC with crc16 field zeroed
  const size_t total = sizeof(MsgHeader) + outHdr.len;
  if (total > 256) return false; // our safety cap (payload limited elsewhere)

  uint8_t tmp[256];
  MsgHeader hdr0 = outHdr; hdr0.crc16 = 0;
  memcpy(tmp, &hdr0, sizeof(MsgHeader));
  memcpy(tmp + sizeof(MsgHeader), inBuf + sizeof(MsgHeader), outHdr.len);

  uint16_t calc = crc16(tmp, total);
  if (calc != outHdr.crc16) return false;

  outPayload = inBuf + sizeof(MsgHeader);
  outPayLen  = outHdr.len;
  return true;
}

} // namespace
