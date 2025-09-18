#include "PizzaPanel.h"
#include <Adafruit_Protomatter.h>

// MatrixPortal S3 default pins (same as your working scroller)
static uint8_t rgbPins[]  = {42,41,40,38,39,37};
static uint8_t addrPins[] = {45,36,48,35}; // 64x32 → A..D only
static uint8_t clockPin=2, latchPin=47, oePin=14;

static Adafruit_Protomatter matrix(64, 4, 1, rgbPins, 4, addrPins,
                                   clockPin, latchPin, oePin, true);

static uint8_t  s_bright  = 100;
static uint8_t  s_style   = 1;     // default: static
static uint8_t  s_speed   = 1;     // 1..5
static String   s_text    = "ONLINE";
static int16_t  s_scrollX = 0;
static uint16_t s_textW   = 0, s_textH = 8;

static inline uint16_t dim565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t rs = (uint16_t)r * s_bright / 255;
  uint16_t gs = (uint16_t)g * s_bright / 255;
  uint16_t bs = (uint16_t)b * s_bright / 255;
  return matrix.color565(rs, gs, bs);
}

bool PizzaPanel::begin64x32(uint8_t brightness) {
  s_bright = brightness;
  auto st = matrix.begin();
  // Proof-of-life border
  matrix.fillScreen(0);
  matrix.drawRect(0,0,63,31, dim565(255,255,255));
  matrix.drawPixel(0,0, dim565(255,0,0));
  matrix.drawPixel(63,31, dim565(0,255,0));
  matrix.show();
  delay(150);
  return st == PROTOMATTER_OK;
}

void PizzaPanel::showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright) {
  if (text) s_text = text;
  s_style  = style;
  s_speed  = constrain(speed,1,5);
  s_bright = bright;

  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setFont(NULL);
  // measure text for scrolling
  int16_t x1,y1;
  matrix.getTextBounds(s_text.c_str(), 0, 0, &x1, &y1, &s_textW, &s_textH);
  s_scrollX = 64; // start off-right for scroll

  // Draw first frame immediately
  matrix.fillScreen(0);
  const int16_t yBase = (32 - 8)/2 + 7; // baseline centered
  matrix.setTextColor(dim565(255,255,255));
  matrix.setCursor((s_style==0)? s_scrollX : 0, yBase);
  matrix.print(s_text);
  matrix.show();
}

void PizzaPanel::loop() {
  // Only animate when style == scroll
  if (s_style != 0) return;
  static uint32_t last=0;
  const uint32_t frameMs = (uint32_t)(1000 / (15 + s_speed*10)); // ~25..65 fps
  if (millis() - last < frameMs) return;
  last = millis();

  matrix.fillScreen(0);
  const int16_t yBase = (32 - 8)/2 + 7;
  matrix.setTextColor(dim565(255,255,255));
  matrix.setTextSize(1);
  matrix.setFont(NULL);
  matrix.setCursor(s_scrollX, yBase);
  matrix.print(s_text);
  matrix.show();

  s_scrollX -= 1;
  if (s_scrollX + (int)s_textW < 0) s_scrollX = 64;
}
