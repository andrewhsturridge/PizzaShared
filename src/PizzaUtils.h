#pragma once
#include <Arduino.h>

#define PZ_LOGI(...)  do { Serial.printf("[INFO] " __VA_ARGS__); Serial.println(); } while(0)
#define PZ_LOGE(...)  do { Serial.printf("[ERR ] " __VA_ARGS__); Serial.println(); } while(0)
#define PZ_LOGD(...)  do { Serial.printf("[DBG ] " __VA_ARGS__); Serial.println(); } while(0)

namespace PizzaUtils {
  struct Ticker {
    uint32_t everyMs;
    uint32_t nextAt;
    void reset(uint32_t ms){ everyMs=ms; nextAt=millis()+ms; }
    bool ready(){ if ((int32_t)(millis()-nextAt) >= 0){ nextAt += everyMs; return true; } return false; }
  };
}
