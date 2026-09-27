#pragma once

#include <cstddef>
#include <cstdint>

namespace lb {

// Glyph metrics for the built-in 8 px font (see firmware/assets/font.txt).
struct GlyphInfo {
  uint32_t codepoint;
  uint16_t offset;  // index of the first column in font_data::kColumns
  uint8_t width;
};

namespace font_data {
extern const uint8_t kColumns[];  // one byte per column, bit 0 = top row
extern const GlyphInfo kGlyphs[];
extern const size_t kGlyphCount;
}  // namespace font_data

constexpr int kFontHeight = 8;
constexpr int kGlyphSpacing = 1;          // blank columns between glyphs
constexpr uint32_t kReplacementChar = 0xFFFD;

// Returns the glyph for a codepoint, or nullptr if the font doesn't have it.
const GlyphInfo* findGlyph(uint32_t codepoint);

inline uint8_t glyphColumn(const GlyphInfo& g, int x) { return font_data::kColumns[g.offset + x]; }

}  // namespace lb
