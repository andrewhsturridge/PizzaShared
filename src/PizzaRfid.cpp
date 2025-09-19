#include "PizzaRfid.h"
#include <SPI.h>
#include <MFRC522.h>

static MFRC522* s_rfid = nullptr;

static bool anyCardPresent() {
  if (!s_rfid) return false;
  byte atqa[2]; byte len = sizeof(atqa);
  MFRC522::StatusCode rc = s_rfid->PICC_WakeupA(atqa, &len);
  if (rc == MFRC522::STATUS_OK || rc == MFRC522::STATUS_COLLISION) return true;
  rc = s_rfid->PICC_RequestA(atqa, &len);
  return (rc == MFRC522::STATUS_OK || rc == MFRC522::STATUS_COLLISION);
}

namespace PizzaRfid {
  bool begin(uint8_t cs, uint8_t rst){
    if (s_rfid) { delete s_rfid; s_rfid=nullptr; }
    s_rfid = new MFRC522(cs, rst);
    s_rfid->PCD_Init();
    s_rfid->PCD_SetAntennaGain(MFRC522::RxGain_max);
    s_rfid->PCD_AntennaOn();
    return true;
  }

  bool readUid(uint8_t* uid, uint8_t& uidLen){
    if (!s_rfid) return false;
    if (!anyCardPresent()) return false;
    if (!s_rfid->PICC_ReadCardSerial()) return false;
    uidLen = s_rfid->uid.size;
    memcpy(uid, s_rfid->uid.uidByte, uidLen);
    s_rfid->PICC_HaltA();
    s_rfid->PCD_StopCrypto1();
    return true;
  }
}
