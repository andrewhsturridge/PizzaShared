#include "PizzaPanel.h"
#include <Adafruit_Protomatter.h>
#include <ctype.h>
#include <math.h>

// -------- Hardware wiring (MatrixPortal S3 defaults) --------
static uint8_t rgbPins[]  = {42,41,40,38,39,37};
static uint8_t addrPins[] = {45,36,48,35}; // 64x32 -> A..D
static uint8_t clockPin=2, latchPin=47, oePin=14;

static Adafruit_Protomatter matrix(64, 3, 1, rgbPins, 4, addrPins,
                                   clockPin, latchPin, oePin, true);

// -------- State --------
static uint8_t  s_bright  = 100;   // global brightness (passed to matrix)
static uint8_t  s_style   = 1;     // 0=marquee,1=static(centered fit),2=wrap,3=vert marquee
static uint8_t  s_speed   = 1;     // 0..5
static String   s_text    = "ONLINE";
static int16_t  s_scrollX = 0;
static uint16_t s_textW   = 0, s_textH = 8; // cached for marquee styles

// Text weight (faux bold): 0=normal,1=bold,2=extra
static uint8_t s_weight = 0;

// Vertical scroll accumulators (style 2)
static float    s_vAccum  = 0.f;
static uint32_t s_vLastMs = 0;
static int      s_barLastCols = -1;   // OTA bottom bar
static const int PANEL_W = 64;
static const int PANEL_H = 32;
static const int BAR_Y   = 31;

// Wrapped vertical text buffers (style 2)
static char     s_linesBlob[256];
static uint16_t s_lineStart[28];
static uint16_t s_lineWidth[28];
static uint8_t  s_lineCount = 0;
static uint8_t  s_lineH     = 9;
static int16_t  s_baseAdj   = 7;
static uint16_t s_blockH    = 0;
static int16_t  s_scrollY   = 0;   // vertical scroll pos (styles 2/3)
static uint8_t  s_vMode     = 0;   // 0=static,1=scroll for style 2

// Current text color (RGB888; converted to 565 on draw)
static uint8_t s_colR = 255, s_colG = 255, s_colB = 255;

// ---------- Internal helpers ----------
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  // Brightness scaled here (0..255)
  uint8_t R = (uint16_t)r * s_bright / 255;
  uint8_t G = (uint16_t)g * s_bright / 255;
  uint8_t B = (uint16_t)b * s_bright / 255;
  return matrix.color565(R, G, B);
}

static inline uint16_t currentColor565() { return rgb565(s_colR, s_colG, s_colB); }

static inline void fontDefaults() {
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setFont(NULL); // 5x7 built-in
}

static inline void printWeighted(int16_t x, int16_t y, const char* s) {
  // base
  matrix.setCursor(x, y);
  matrix.print(s);
  if (s_weight >= 1) {
    matrix.setCursor(x + 1, y);
    matrix.print(s);
  }
  if (s_weight >= 2) {
    matrix.setCursor(x, y + 1);
    matrix.print(s);
  }
}

// Measure width/height at current font size
static void getBounds(const char* s, uint16_t& w, uint16_t& h, int16_t& x1, int16_t& y1) {
  matrix.getTextBounds((char*)s, 0, 0, &x1, &y1, &w, &h);
}

static uint16_t measureW(const char* s) {
  int16_t x1, y1; uint16_t w, h; getBounds(s, w, h, x1, y1);
  return w;
}

static void computeFontMetrics() {
  fontDefaults();
  int16_t x1, y1; uint16_t w, h;
  matrix.getTextBounds((char*)"Ay", 0, 0, &x1, &y1, &w, &h);
  s_lineH   = h + 1;
  s_baseAdj = -y1;
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

static void drawStaticBlock() {
  matrix.fillScreen(0);
  matrix.setTextColor(currentColor565());
  fontDefaults();
  for (uint8_t i=0; i<s_lineCount; ++i) {
    const char* ln = &s_linesBlob[s_lineStart[i]];
    uint16_t w = s_lineWidth[i];
    int16_t x = (int16_t)((PANEL_W - w)/2); if (x < 0) x = 0;
    int16_t yTop  = (int16_t)((PANEL_H - s_blockH)/2);
    int16_t yBase = yTop + s_baseAdj + i * s_lineH;
    printWeighted(x, yBase, ln);
  }
  matrix.show();
}

static void drawScrolledBlock(int16_t yTop) {
  matrix.fillScreen(0);
  matrix.setTextColor(currentColor565());
  fontDefaults();
  for (uint8_t i=0; i<s_lineCount; ++i) {
    const char* ln = &s_linesBlob[s_lineStart[i]];
    uint16_t w = s_lineWidth[i];
    int16_t x = (int16_t)((PANEL_W - w)/2); if (x < 0) x = 0;
    int16_t yBase = yTop + s_baseAdj + i * s_lineH;
    if (yBase < -s_lineH || yBase > (PANEL_H + s_lineH)) continue;
    printWeighted(x, yBase, ln);
  }
  matrix.show();
}

// -------- Fit-to-screen (static) with centered layout (style 1) --------
static bool fitAndCenterSingleLine(const String& s) {
  // Try sizes from large to small (built-in font scaled with setTextSize)
  matrix.setTextWrap(false);
  int16_t x1, y1; uint16_t w, h;
  for (int sz = 10; sz >= 1; --sz) {
    matrix.setTextSize(sz);
    matrix.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (w <= PANEL_W - 2 && h <= PANEL_H - 2) {
      // Center & draw
      matrix.fillScreen(0);
      int16_t x = (int16_t)((PANEL_W - w)/2) - x1;
      int16_t y = (int16_t)((PANEL_H - h)/2) - y1;
      matrix.setCursor(x, y);
      matrix.setTextColor(currentColor565());
      printWeighted(x, y, s.c_str()); // printWeighted sets cursor internally; pass coords
      matrix.show();
      return true;
    }
  }
  return false; // didn’t fit even at size 1
}

// ---------- Public API ----------
namespace PizzaPanel {

bool begin64x32(uint8_t brightness) {
  s_bright = brightness;
  auto st = matrix.begin();

  // Proof-of-life border
  matrix.fillScreen(0);
  matrix.drawRect(0,0,PANEL_W-1,PANEL_H-1, rgb565(255,255,255));
  matrix.drawPixel(0,0,   rgb565(255,0,0));
  matrix.drawPixel(PANEL_W-1,PANEL_H-1, rgb565(0,255,0));
  matrix.show();
  delay(150);
  return st == PROTOMATTER_OK;
}

void setBrightness(uint8_t brightness) {
  s_bright = brightness;
}

void setWeight(uint8_t weight) { s_weight = (weight > 2) ? 2 : weight; }
void setColor(uint8_t r, uint8_t g, uint8_t b) { s_colR = r; s_colG = g; s_colB = b; }

void showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright) {
  if (text) s_text = text;
  s_style  = style;
  s_speed  = (speed > 5) ? 5 : speed;
  setBrightness(bright);

  fontDefaults();
  matrix.setTextColor(currentColor565());

  // STYLE 2: wrapped vertical (static if block fits; else bottom->top scroll)
  if (s_style == 2) {
    computeFontMetrics();
    wrapToWidth(s_text.c_str(), PANEL_W - 2);
    s_vAccum  = 0.f;
    s_vLastMs = millis();

    matrix.fillScreen(0);

    if (s_blockH <= PANEL_H) {
      s_vMode = 0;
      drawStaticBlock();
    } else {
      int16_t yTop = PANEL_H - (int16_t)s_blockH; // start bottom-aligned
      drawScrolledBlock(yTop);
      s_vMode   = 1;
      s_scrollY = PANEL_H; // loop() will scroll up
    }
    return;
  }

  // STYLE 3: single-line vertical marquee (bottom->top)
  if (s_style == 3) {
    int16_t x1,y1; matrix.getTextBounds(s_text.c_str(), 0,0, &x1,&y1, &s_textW,&s_textH);
    s_scrollY = PANEL_H + (int16_t)s_textH;
    matrix.fillScreen(0);
    fontDefaults();
    int16_t x = (int16_t)((PANEL_W - (int)s_textW)/2); if (x < 0) x = 0;
    printWeighted(x, s_scrollY, s_text.c_str());
    matrix.show();
    return;
  }

  // STYLE 1: static (now: fit to screen & centered; else fallback to marquee)
  if (s_style == 1) {
    if (fitAndCenterSingleLine(s_text)) return;
    // fallback to marquee
    s_style = 0;
  }

  // STYLE 0: horizontal marquee
  {
    int16_t x1,y1; matrix.getTextBounds(s_text.c_str(), 0,0, &x1,&y1, &s_textW,&s_textH);
    const int16_t yBase = (PANEL_H - 8)/2 + 7; // baseline for size=1
    s_scrollX = PANEL_W;
    matrix.fillScreen(0);
    printWeighted(s_scrollX, yBase, s_text.c_str());
    matrix.show();
    return;
  }
}

void loop() {
  // style 0: horizontal marquee
  if (s_style == 0) {
    static uint32_t last=0;
    const uint32_t frameMs = (uint32_t)(1000 / (15 + s_speed*10)); // ~25..65 fps
    uint32_t now = millis();
    if (now - last < frameMs) return;
    last = now;

    matrix.fillScreen(0);
    const int16_t yBase = (PANEL_H - 8)/2 + 7;
    matrix.setTextColor(currentColor565());
    fontDefaults();
    printWeighted(s_scrollX, yBase, s_text.c_str());
    matrix.show();

    s_scrollX -= 1;
    if (s_scrollX + (int)s_textW < 0) s_scrollX = PANEL_W;
    return;
  }

  // style 2: wrapped vertical scroll
  if (s_style == 2 && s_vMode == 1) {
    static uint32_t dwell = 0;
    uint32_t now = millis();
    float dt = (now - s_vLastMs) / 1000.0f;
    s_vLastMs = now;

    static const float PXPS[6] = { 3.f, 6.f, 10.f, 15.f, 20.f, 24.f };
    uint8_t idx = (s_speed > 5) ? 5 : s_speed;
    float v = PXPS[idx];

    s_vAccum += v * dt;
    int steps = (int)floorf(s_vAccum);
    if (steps >= 1) {
      s_vAccum -= steps;
      s_scrollY -= steps;           // move up
      drawScrolledBlock(s_scrollY);
    }

    if (s_scrollY <= -(int16_t)s_blockH) {
      if (!dwell) { dwell = now; return; }
      if (now - dwell > 700) { dwell = 0; s_scrollY = PANEL_H; }
    }
    return;
  }

  // style 3: single-line vertical marquee
  if (s_style == 3) {
    static uint32_t last = 0;
    const uint32_t frameMs = (uint32_t)(1000 / (15 + s_speed*10));
    uint32_t now = millis();
    if (now - last < frameMs) return;
    last = now;

    matrix.fillScreen(0);
    matrix.setTextColor(currentColor565());
    fontDefaults();
    int16_t x = (int16_t)((PANEL_W - (int)s_textW)/2); if (x < 0) x = 0;
    printWeighted(x, s_scrollY, s_text.c_str());
    matrix.show();

    s_scrollY -= 1;
    if (s_scrollY + (int)s_textH < 0) {
      s_scrollY = PANEL_H + s_textH;
    }
    return;
  }
}

void progressBarReset() {
  matrix.fillScreen(0);
  matrix.show();
  s_barLastCols = -1;
}

void showBottomBarPercent(uint8_t percent) {
  if (percent > 100) percent = 100;

  uint8_t step = percent / 20;            // 0..5
  int cols = (step * PANEL_W) / 5;        // 0..64

  if (s_barLastCols < 0) {
    matrix.drawFastHLine(0, BAR_Y, PANEL_W, 0);
    s_barLastCols = 0;
  }

  if (cols == s_barLastCols) return;

  if (cols > s_barLastCols) {
    matrix.drawFastHLine(s_barLastCols, BAR_Y, cols - s_barLastCols, rgb565(0,255,0));
  } else {
    matrix.drawFastHLine(cols, BAR_Y, s_barLastCols - cols, 0);
  }

  s_barLastCols = cols;
  matrix.show();
}

void show() { matrix.show(); }
Adafruit_GFX& gfx() { return (Adafruit_GFX&)matrix; }

} // namespace PizzaPanel
