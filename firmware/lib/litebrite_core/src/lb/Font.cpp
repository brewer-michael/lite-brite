#include "lb/Font.h"

namespace lb {

const GlyphInfo* findGlyph(uint32_t codepoint) {
  size_t lo = 0;
  size_t hi = font_data::kGlyphCount;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const uint32_t cp = font_data::kGlyphs[mid].codepoint;
    if (cp == codepoint) return &font_data::kGlyphs[mid];
    if (cp < codepoint) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return nullptr;
}

}  // namespace lb
