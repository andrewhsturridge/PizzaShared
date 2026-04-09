#pragma once
#include <Arduino.h>

namespace PizzaRfid {
  bool begin(uint8_t cs, uint8_t rst);
  // Returns true if *any* card is present in the RF field (even if UID read fails).
  // Use this for "is the card removed yet?" logic.
  bool present();
  bool readUid(uint8_t* uid, uint8_t& uidLen); // returns true if a card is present and uid read
}
