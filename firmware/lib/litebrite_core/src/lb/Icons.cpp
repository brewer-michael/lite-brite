#include "lb/Icons.h"

#include <cstring>

namespace lb {

int findIconByName(std::string_view name) {
  size_t lo = 0;
  size_t hi = icon_data::kNameCount;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const std::string_view candidate(icon_data::kNames[mid].name);
    const int cmp = candidate.compare(name);
    if (cmp == 0) return icon_data::kNames[mid].icon;
    if (cmp < 0) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return -1;
}

int findIconByCodepoint(uint32_t codepoint) {
  size_t lo = 0;
  size_t hi = icon_data::kEmojiCount;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const EmojiRange& r = icon_data::kEmoji[mid];
    if (codepoint < r.first) {
      hi = mid;
    } else if (codepoint > r.last) {
      lo = mid + 1;
    } else {
      return r.icon;
    }
  }
  return -1;
}

const IconInfo& iconInfo(int icon) { return icon_data::kIcons[icon]; }

uint8_t iconPixel(int icon, int x, int y) {
  const IconInfo& info = icon_data::kIcons[icon];
  if (x < 0 || y < 0 || x >= info.width || y >= kIconHeight) return kIconTransparent;
  return icon_data::kPixels[info.offset + y * info.width + x];
}

Rgb iconPaletteColor(uint8_t pixel) { return icon_data::kPalette[pixel - 2]; }

}  // namespace lb
