#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "lb/Color.h"

namespace lb {

// Decodes the UTF-8 codepoint starting at s[i] and advances i past it.
// Malformed or truncated sequences yield U+FFFD and advance one byte.
uint32_t decodeUtf8(std::string_view s, size_t& i);

enum class ItemKind : uint8_t { Glyph, Icon };

struct TextItem {
  ItemKind kind;
  uint16_t ref;     // index into font_data::kGlyphs or icon_data::kIcons
  uint8_t width;    // in font pixels
  bool colored;     // true when {color} markup applies to this item
  Rgb color;        // the {color}, if colored
  int16_t x;        // left edge within the line, in font pixels
};

// One line of text laid out in font pixels.
//
// Markup understood in message text:
//   :key:        built-in icon by name (see firmware/assets/icons.txt)
//   {orange}     switch the text color (name or 6-digit hex, e.g. {ff8800})
//   {}           back to the message's color
//   {{           a literal '{'
// Emoji with a matching icon (🔑 ❤️ 🏠 🙂 ...) become that icon, other emoji are
// dropped, accented letters and "smart" punctuation fold to plain ASCII, and
// runs of whitespace collapse to a single space.
class TextLine {
 public:
  void set(std::string_view text);

  int width() const { return width_; }
  bool empty() const { return items_.empty(); }
  const std::vector<TextItem>& items() const { return items_; }

 private:
  void emitGlyph(uint32_t codepoint);
  void emitIcon(int icon);
  void push(ItemKind kind, uint16_t ref, uint8_t width);

  std::vector<TextItem> items_;
  int width_ = 0;
  bool pendingSpace_ = false;
  bool colored_ = false;
  Rgb color_;
};

}  // namespace lb
