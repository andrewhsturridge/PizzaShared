#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "PizzaProtocol.h"  // for Role enum

#ifndef PIZZA_ROLE
  #define PIZZA_ROLE CENTRAL
#endif
#ifndef PIZZA_HOUSE_ID
  #define PIZZA_HOUSE_ID 0
#endif
#ifndef FW_VERSION
  #define FW_VERSION "0.1.0"
#endif

namespace PizzaIdentity {
  inline Role role()        { return (Role)PIZZA_ROLE; }
  inline uint8_t houseId()  { return (uint8_t)PIZZA_HOUSE_ID; }
  inline const char* fw()   { return FW_VERSION; }

  inline void mac(uint8_t out[6]) { WiFi.macAddress(out); }
  inline String macStr() {
    uint8_t m[6]; mac(m);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
    return String(buf);
  }
}
