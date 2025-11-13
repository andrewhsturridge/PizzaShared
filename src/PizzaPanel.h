#pragma once
#include <stdint.h>
#include <Adafruit_GFX.h>

namespace PizzaPanel {

// Initialize MatrixPortal S3 64x32 display. Returns true on success.
bool begin64x32(uint8_t brightness);

// High-level text API
// style: 0 = horizontal marquee, 1 = static (now auto-fit + centered), 2 = wrapped vertical,
//        3 = single-line vertical marquee (bottom -> top)
// speed: 0..5 (used by marquee styles)
void showText(const char* text, uint8_t style, uint8_t speed, uint8_t bright);

// Call regularly from loop(); advances marquee/scroll styles (0,2,3).
void loop();

// OTA progress helpers
void progressBarReset();
void showBottomBarPercent(uint8_t percent);

// Appearance controls
void setWeight(uint8_t weight /*0..2*/);          // faux-bold 0=normal,1=bold,2=extra
void setColor(uint8_t r, uint8_t g, uint8_t b);   // text color
void setBrightness(uint8_t brightness);           // 0..255 global panel brightness

// Low-level helpers (optional)
// show(): flush the current frame to the panel (Protomatter .show()).
void show();

// gfx(): get an Adafruit_GFX reference to the panel for custom drawing.
Adafruit_GFX& gfx();

} // namespace PizzaPanel
