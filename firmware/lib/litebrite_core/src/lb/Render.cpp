#include "lb/Render.h"

#include "lb/Font.h"
#include "lb/Icons.h"

namespace lb {

namespace {

constexpr int kRainbowDegreesPerPixel = 10;

void fillBlock(Canvas& canvas, int x, int y, int s, Rgb c) {
  for (int dy = 0; dy < s; ++dy) {
    for (int dx = 0; dx < s; ++dx) canvas.set(x + dx, y + dy, c);
  }
}

Rgb textColorAt(const TextItem& item, const TextStyle& style, int screenX) {
  if (item.colored) return item.color;
  if (style.rainbow) return hsv(style.rainbowPhase + screenX * kRainbowDegreesPerPixel, 255, 255);
  return style.color;
}

Rgb finish(Rgb c, const TextStyle& style) {
  if (style.inverse) return colors::kBlack;
  return style.level == 255 ? c : scale(c, style.level);
}

}  // namespace

int textScale(const Canvas& canvas) {
  const int s = canvas.height() / kFontHeight;
  return s < 1 ? 1 : s;
}

int textTop(const Canvas& canvas) { return (canvas.height() - kFontHeight * textScale(canvas)) / 2; }

void drawText(Canvas& canvas, const TextLine& line, int x0, const TextStyle& style) {
  const int s = textScale(canvas);
  const int top = textTop(canvas);
  for (const TextItem& item : line.items()) {
    const int left = x0 + item.x * s;
    if (left >= canvas.width() || left + item.width * s <= 0) continue;
    if (item.kind == ItemKind::Glyph) {
      const GlyphInfo& g = font_data::kGlyphs[item.ref];
      for (int col = 0; col < g.width; ++col) {
        const uint8_t bits = glyphColumn(g, col);
        if (bits == 0) continue;
        const int x = left + col * s;
        const Rgb c = finish(textColorAt(item, style, x), style);
        for (int row = 0; row < kFontHeight; ++row) {
          if (bits & (1u << row)) fillBlock(canvas, x, top + row * s, s, c);
        }
      }
    } else {
      for (int col = 0; col < item.width; ++col) {
        const int x = left + col * s;
        for (int row = 0; row < kIconHeight; ++row) {
          const uint8_t p = iconPixel(item.ref, col, row);
          if (p == kIconTransparent) continue;
          const Rgb c = p == kIconTextColor ? textColorAt(item, style, x) : iconPaletteColor(p);
          fillBlock(canvas, x, top + row * s, s, finish(c, style));
        }
      }
    }
  }
}

void drawWiringTest(Canvas& canvas, uint32_t tMs) {
  canvas.clear();
  const int w = canvas.width();
  const int h = canvas.height();
  const uint32_t n = static_cast<uint32_t>(w * h);
  const uint32_t i = (tMs / 60) % n;
  canvas.set(static_cast<int>(i % w), static_cast<int>(i / w), colors::kWhite);
  canvas.set(0, 0, colors::kRed);
  canvas.set(w - 1, 0, colors::kGreen);
  canvas.set(0, h - 1, Rgb(0, 0, 255));
}

}  // namespace lb
