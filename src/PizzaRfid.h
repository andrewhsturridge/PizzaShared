#pragma once
#include <Arduino.h>

namespace PizzaRfid {
  bool begin(uint8_t cs, uint8_t rst);
  bool readUid(uint8_t* uid, uint8_t& uidLen); // returns true if a card is present and uid read
}
