#pragma once

#include <cstdint>

#include "lb/Canvas.h"
#include "lb/Text.h"

namespace lb {

struct TextStyle {
  Rgb color = colors::kWhite;  // base text color
  bool rainbow = false;        // color each column by hue instead
  int rainbowPhase = 0;        // hue offset in degrees
  uint8_t level = 255;         // intensity multiplier (pulse)
  bool inverse = false;        // draw lit pixels black (for flash frames)
};

// Integer pixel scale for the canvas: 8 px tall text on an 8-row matrix,
// doubled on a 16-row one, and so on.
int textScale(const Canvas& canvas);

// Top row of the text band (text is vertically centered).
int textTop(const Canvas& canvas);

// Draws `line` with its left edge at canvas column x0. Items entirely off the
// canvas are skipped, so scrolling long text is cheap.
void drawText(Canvas& canvas, const TextLine& line, int x0, const TextStyle& style);

// Diagnostic pattern for checking matrix wiring: red top-left, green
// top-right, blue bottom-left corners and a white dot walking the rows
// left-to-right, top-to-bottom.
void drawWiringTest(Canvas& canvas, uint32_t tMs);

}  // namespace lb
