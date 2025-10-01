#include "PizzaPanel.h"
#include <Adafruit_Protomatter.h>
#include <ctype.h>
#include <math.h>

// MatrixPortal S3 default pins (same as your working scroller)
static uint8_t rgbPins[]  = {42,41,40,38,39,37};
static uint8_t addrPins[] = {45,36,48,35}; // 64x32 → A..D only
static uint8_t clockPin=2, latchPin=47, oePin=14;

static Adafruit_Protomatter matrix(64, 3, 1, rgbPins, 4, addrPins,
                                   clockPin, latchPin, oePin, true);

static uint8_t  s_bright  = 100;
static uint8_t  s_style   = 1;     // default: static
static uint8_t  s_speed   = 1;     // 1..5
static String   s_text    = "ONLINE";
static int16_t  s_scrollX = 0;
static uint16_t s_textW   = 0, s_textH = 8;

// Text weight (stroke fattening): 0=normal, 1=bold, 2=extra bold
static uint8_t s_weight = 0;

// Accumulator-based timing for vertical scroll (style 2)
static float    s_vAccum  = 0.f;
static uint32_t s_vLastMs = 0;

static int  s_barLastCols = -1;        // last lit pixel count (0..64), -1 = uninit
static const int BAR_Y = 31;           // bottom row (0-based), 1-px tall

// --- Add near existing statics (s_text, s_style, s_speed, etc.) ---
static char     s_linesBlob[256];     // wrapped lines blob (NUL-separated)
static uint16_t s_lineStart[28];      // line offsets into blob
static uint16_t s_lineWidth[28];      // px widths for centering
static uint8_t  s_lineCount = 0;
static uint8_t  s_lineH     = 9;      // default; recomputed at runtime
static int16_t  s_baseAdj   = 7;      // baseline lift; recomputed at runtime
static uint16_t s_blockH    = 0;      // total wrapped height
static int16_t  s_scrollY   = 0;      // vertical scroll position
static uint8_t  s_vMode     = 0;      // 0=static, 1=scroll (used when style==2)

// Current text color (default white)
static uint8_t s_colR = 255, s_colG = 255, s_colB = 255;

// Brightness-scaled RGB565 helper
static inline uint16_t dim565(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t R = (uint16_t)r * s_bright / 255;
  uint8_t G = (uint16_t)g * s_bright / 255;
  uint8_t B = (uint16_t)b * s_bright / 255;
  return matrix.color565(R, G, B);
}

// Convenience for current text color
static inline uint16_t currentColor565() { return dim565(s_colR, s_colG, s_colB); }

// Draw text with faux-bold passes (keeps the exact BOOT font)
static inline void printWeighted(int16_t x, int16_t y, const char* s) {
  // base
  matrix.setCursor(x, y);
  matrix.print(s);
  if (s_weight >= 1) {            // horizontal fattening
    matrix.setCursor(x + 1, y);
    matrix.print(s);
  }
  if (s_weight >= 2) {            // add a touch of vertical fattening
    matrix.setCursor(x, y + 1);
    matrix.print(s);
  }
}

static inline void fontDefaults() {
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setFont(NULL); // EXACT same font as HousePanel uses
}

static void computeFontMetrics() {
  fontDefaults();
  int16_t x1, y1; uint16_t w, h;
  matrix.getTextBounds((char*)"Ay", 0, 0, &x1, &y1, &w, &h);
  s_lineH   = h + 1;  // add a pixel of spacing
  s_baseAdj = -y1;
}

static uint16_t measureW(const char* s) {
  int16_t x1, y1; uint16_t w, h;
  matrix.getTextBounds((char*)s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

static void wrapToWidth(const char* s, uint8_t maxW) {
  memset(s_linesBlob, 0, sizeof(s_linesBlob));
  s_lineCount = 0;
  uint16_t blobPos = 0;

  char line[256] = {0};  uint16_t ll = 0;
  char word[256] = {0};  uint16_t wl = 0;

  auto flushLine = [&](){
    if (!ll) return;
    if (s_lineCount < 28 && blobPos + ll + 1 < sizeof(s_linesBlob)) {
      s_lineStart[s_lineCount] = blobPos;
      memcpy(&s_linesBlob[blobPos], line, ll);
      s_linesBlob[blobPos + ll] = '\0';
      s_lineWidth[s_lineCount] = measureW(&s_linesBlob[blobPos]);
      s_lineCount++;
      blobPos += ll + 1;
    }
    line[0]=0; ll=0;
  };

  const char* p = s;
  while (true) {
    char c = *p;
    bool atEnd = (c == '\0');
    if (atEnd || isspace((unsigned char)c)) {
      if (wl) {
        char cand[256];
        if (ll) snprintf(cand, sizeof(cand), "%s %s", line, word);
        else    snprintf(cand, sizeof(cand), "%s", word);
        uint16_t w = measureW(cand);
        if (w <= maxW) {
          if (ll) { line[ll++] = ' '; line[ll]=0; }
          strlcpy(&line[ll], word, sizeof(line)-ll); ll += wl;
        } else {
          flushLine();
          strlcpy(line, word, sizeof(line)); ll = wl;
        }
        word[0]=0; wl=0;
      }
      if (atEnd) break;
    } else {
      if (wl+1 < sizeof(word)) { word[wl++] = c; word[wl] = 0; }
    }
    ++p;
  }
  flushLine();

  if (s_lineCount == 0) {
    s_lineStart[0] = 0; s_linesBlob[0] = '\0'; s_lineWidth[0] = 0; s_lineCount = 1;
  }
  s_blockH = s_lineCount * s_lineH;
}

// drawStaticBlock()
static void drawStaticBlock() {
  matrix.fillScreen(0);
  matrix.setTextColor(currentColor565());
  fontDefaults();
  for (uint8_t i=0; i<s_lineCount; ++i) {
    const char* ln = &s_linesBlob[s_lineStart[i]];
    uint16_t w = s_lineWidth[i];
    int16_t x = (int16_t)((64 - w)/2); if (x < 0) x = 0;
    int16_t yTop  = (int16_t)((32 - s_blockH)/2);
    int16_t yBase = yTop + s_baseAdj + i * s_lineH;
    printWeighted(x, yBase, ln);
  }
  matrix.show();
}

// drawScrolledBlock()
static void drawScrolledBlock(int16_t yTop) {
  matrix.fillScreen(0);
  matrix.setTextColor(currentColor565());
  fontDefaults();
  for (uint8_t i=0; i<s_lineCount; ++i) {
    const char* ln = &s_linesBlob[s_lineStart[i]];
    uint16_t w = s_lineWidth[i];
    int16_t x = (int16_t)((64 - w)/2); if (x < 0) x = 0;
    int16_t yBase = yTop + s_baseAdj + i * s_lineH;
    if (yBase < -s_lineH || yBase > (32 + s_lineH)) continue;
    printWeighted(x, yBase, ln);
  }
  matrix.show();
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
  s_speed  = constrain(speed, 0, 5);   // allow 0 if you enabled extra-slow
  s_bright = bright;

  fontDefaults();
  matrix.setTextColor(currentColor565());

  // --------- STYLE 2: wrapped vertical (static if fits; else bottom→top) ---------
  if (s_style == 2) {
    computeFontMetrics();
    wrapToWidth(s_text.c_str(), 64 - 2);

    // init accumulators if you added extra-slow support
    s_vAccum  = 0.f;
    s_vLastMs = millis();

    matrix.fillScreen(0);

    if (s_blockH <= 32) {
      s_vMode = 0;
      drawStaticBlock();
    } else {
      // bottom-aligned first frame (no centered flash)
      int16_t yTop = 32 - (int16_t)s_blockH;
      drawScrolledBlock(yTop);
      s_vMode   = 1;
      s_scrollY = 32;  // loop() will scroll up from the bottom
    }
    return;
  }

  // --------- STYLE 3: single-line vertical marquee (bottom→top) ---------
  if (s_style == 3) {
    int16_t x1,y1; matrix.getTextBounds(s_text.c_str(), 0, 0, &x1, &y1, &s_textW, &s_textH);
    s_scrollY = 32 + (int16_t)s_textH;

    matrix.fillScreen(0);
    fontDefaults();
    int16_t x = (int16_t)((64 - (int)s_textW)/2); if (x < 0) x = 0;
    printWeighted(x, s_scrollY, s_text.c_str());
    matrix.show();
    return;
  }

  // --------- STYLE 0/1 (horizontal marquee / static) ---------
  int16_t x1,y1; matrix.getTextBounds(s_text.c_str(), 0, 0, &x1, &y1, &s_textW, &s_textH);
  const int16_t yBase = (32 - 8)/2 + 7;  // same baseline as before
  s_scrollX = 64;

  matrix.fillScreen(0);

  if (s_style == 0) { // horizontal marquee
    printWeighted(s_scrollX, yBase, s_text.c_str());
    matrix.show();
    return;
  }

  if (s_style == 1) { // static
    printWeighted(0, yBase, s_text.c_str());
    matrix.show();
    return;
  }

  // Fallback
  printWeighted(0, yBase, s_text.c_str());
  matrix.show();
}


void PizzaPanel::loop() {
  // style 0: horizontal marquee (existing behavior)
  if (s_style == 0) {
    static uint32_t last=0;
    const uint32_t frameMs = (uint32_t)(1000 / (15 + s_speed*10)); // ~25..65 fps
    if (millis() - last < frameMs) return;
    last = millis();

    matrix.fillScreen(0);
    const int16_t yBase = (32 - 8)/2 + 7;
	matrix.setTextColor(currentColor565());
    fontDefaults();
	printWeighted(s_scrollX, yBase, s_text.c_str());
    matrix.show();

    s_scrollX -= 1;
    if (s_scrollX + (int)s_textW < 0) s_scrollX = 64;
    return;
  }

  // style 2: wrapped vertical scroll
  if (s_style == 2 && s_vMode == 1) {
    // Extra-slow accumulator version (works even slower than speed=1)
    // If you didn't add s_vAccum/s_vLastMs statics yet, see Block 3 below.
    static uint32_t dwell = 0;

    uint32_t now = millis();
    float dt = (now - s_vLastMs) / 1000.0f;
    s_vLastMs = now;

    // Pixels-per-second for speeds 0..5 (tweak to taste)
    static const float PXPS[6] = { 3.f, 6.f, 10.f, 15.f, 20.f, 24.f };
    uint8_t idx = (s_speed > 5) ? 5 : s_speed;
    float v = PXPS[idx];

    s_vAccum += v * dt;
    int steps = (int)floorf(s_vAccum);
    if (steps >= 1) {
      s_vAccum -= steps;
      s_scrollY -= steps;             // move up
      drawScrolledBlock(s_scrollY);   // <-- your existing helper
    }

    if (s_scrollY <= -(int16_t)s_blockH) {
      if (!dwell) { dwell = now; return; }
      if (now - dwell > 700) { dwell = 0; s_scrollY = 32; }
    }
    return;
  }
  // style 3: single-line vertical marquee (bottom -> top)
  if (s_style == 3) {
    static uint32_t last = 0;
    const uint32_t frameMs = (uint32_t)(1000 / (15 + s_speed*10)); // same pacing as style 0
    uint32_t now = millis();
    if (now - last < frameMs) return;
    last = now;

    matrix.fillScreen(0);
	matrix.setTextColor(currentColor565());
    fontDefaults();
    int16_t x = (int16_t)((64 - (int)s_textW)/2); if (x < 0) x = 0;  // center; set x=0 for left align
	printWeighted(x, s_scrollY, s_text.c_str());
    matrix.show();

    s_scrollY -= 1;                              // move up 1 px per frame
    if (s_scrollY + (int)s_textH < 0) {          // fully off the top?
      s_scrollY = 32 + s_textH;                  // restart from bottom
    }
    return;
  }
}

void PizzaPanel::progressBarReset() {
  // Full-frame blackout so any prior text is gone
  matrix.fillScreen(0);
  matrix.show();

  // Force bar re-init on next call
  s_barLastCols = -1;
}

void PizzaPanel::showBottomBarPercent(uint8_t percent) {
  if (percent > 100) percent = 100;

  // Map to coarse 20% steps: 0,20,40,60,80,100 -> 0..64 columns
  uint8_t step = percent / 20;         // 0..5
  int cols = (step * 64) / 5;          // integer pixels to light

  // First frame: make sure the bottom row is black once
  if (s_barLastCols < 0) {
    // clear just the bottom row (no full-screen clear)
    matrix.drawFastHLine(0, BAR_Y, 64, 0);
    s_barLastCols = 0;
  }

  if (cols == s_barLastCols) return;   // nothing new to draw

  if (cols > s_barLastCols) {
    // grow: draw only the newly added green slice
    matrix.drawFastHLine(s_barLastCols, BAR_Y, cols - s_barLastCols, matrix.color565(0,255,0));
  } else {
    // shrink (shouldn't happen in OTA, but keep it correct)
    matrix.drawFastHLine(cols, BAR_Y, s_barLastCols - cols, 0);
  }

  s_barLastCols = cols;
  matrix.show();                        // one full-frame present per 20% milestone
}

void PizzaPanel::setWeight(uint8_t weight) { s_weight = (weight > 2) ? 2 : weight; }

void PizzaPanel::setColor(uint8_t r, uint8_t g, uint8_t b) {
  s_colR = r; s_colG = g; s_colB = b;
}

