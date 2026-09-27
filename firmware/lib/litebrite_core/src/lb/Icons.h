#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "lb/Color.h"

namespace lb {

// Built-in pixel-art icons (see firmware/assets/icons.txt).
struct IconInfo {
  const char* name;
  uint16_t offset;  // index of the first pixel in icon_data::kPixels
  uint8_t width;
};

struct IconName {
  const char* name;  // icon name or alias
  uint8_t icon;      // index into icon_data::kIcons
};

struct EmojiRange {
  uint32_t first;
  uint32_t last;
  uint8_t icon;
};

namespace icon_data {
extern const Rgb kPalette[];
extern const uint8_t kPixels[];
extern const IconInfo kIcons[];
extern const size_t kIconCount;
extern const IconName kNames[];
extern const size_t kNameCount;
extern const EmojiRange kEmoji[];
extern const size_t kEmojiCount;
}  // namespace icon_data

constexpr int kIconHeight = 8;

// Pixel values: 0 = transparent, 1 = current text color, n >= 2 = kPalette[n - 2].
constexpr uint8_t kIconTransparent = 0;
constexpr uint8_t kIconTextColor = 1;

int findIconByName(std::string_view name);      // -1 if unknown
int findIconByCodepoint(uint32_t codepoint);    // -1 if no icon for it
const IconInfo& iconInfo(int icon);
uint8_t iconPixel(int icon, int x, int y);
Rgb iconPaletteColor(uint8_t pixel);            // for pixel values >= 2

}  // namespace lb
