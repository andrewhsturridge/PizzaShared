#include "PizzaWindowLeds.h"
#ifdef PIZZA_ENABLE_LEDS_MODULE
  #include <Adafruit_NeoPixel.h>
  static Adafruit_NeoPixel strip;
  static uint32_t s_color=0; static uint16_t onMs=0, offMs=0;
  static bool on=false; static uint32_t t=0;
#endif

namespace PizzaWindowLeds {
#ifdef PIZZA_ENABLE_LEDS_MODULE
  bool begin(uint8_t pin, uint16_t count){
    strip = Adafruit_NeoPixel(count, pin, NEO_GRB + NEO_KHZ800);
    strip.begin(); strip.show(); return true;
  }
  uint32_t rgb(uint8_t r, uint8_t g, uint8_t b){ return strip.Color(r,g,b); } // <-- new
  void blink(uint32_t color, uint16_t msOn, uint16_t msOff){
    s_color=color; onMs=msOn; offMs=msOff; on=false; t=millis();
  }
  void loop(){
    uint32_t now=millis();
    if (!on && (now - t) >= offMs){ on=true; t=now; for (uint16_t i=0;i<strip.numPixels();i++) strip.setPixelColor(i,s_color); strip.show(); }
    else if (on && (now - t) >= onMs){ on=false; t=now; for (uint16_t i=0;i<strip.numPixels();i++) strip.setPixelColor(i,0); strip.show(); }
  }
#else
  bool begin(uint8_t, uint16_t){ return true; }
  uint32_t rgb(uint8_t r, uint8_t g, uint8_t b){ return (uint32_t)r<<16 | (uint32_t)g<<8 | b; }
  void blink(uint32_t, uint16_t, uint16_t){}
  void loop(){}
#endif
}
