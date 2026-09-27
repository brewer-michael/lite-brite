#include "lb/Text.h"

#include "lb/Font.h"
#include "lb/Icons.h"

namespace lb {

namespace {

constexpr size_t kMaxMarkup = 24;  // longest :icon: or {color} name we look for

bool isSpace(uint32_t cp) {
  return cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
         (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 || cp == 0x202F ||
         cp == 0x205F || cp == 0x3000;
}

// Characters that never render: controls, joiners, variation selectors,
// skin-tone modifiers, keycap/tag sequences, BOM, soft hyphen.
bool isIgnorable(uint32_t cp) {
  return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) || cp == 0xAD || (cp >= 0x200B && cp <= 0x200F) ||
         cp == 0x2060 || (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0xFEFF || cp == 0x20E3 ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0000 && cp <= 0xE007F);
}

// Pictographs we don't have an icon for are dropped rather than shown as a box.
bool isEmojiLike(uint32_t cp) {
  return (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0x2190 && cp <= 0x21FF) ||
         (cp >= 0x2300 && cp <= 0x23FF) || (cp >= 0x25A0 && cp <= 0x27BF) ||
         (cp >= 0x2B00 && cp <= 0x2BFF) || cp == 0x3030 || cp == 0x303D || cp == 0x3297 ||
         cp == 0x3299 || (cp >= 0xE000 && cp <= 0xF8FF) || cp >= 0xF0000;
}

// U+00C0..U+00FF
const char* const kLatin1Fold[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I",  "I",
    "D", "N", "O", "O", "O", "O", "O",  "x", "O", "U", "U", "U", "U", "Y", "Th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i",  "i",
    "d", "n", "o", "o", "o", "o", "o",  "/", "o", "u", "u", "u", "u", "y", "th", "y",
};

// U+0100..U+017F, one letter each.
constexpr char kLatinExtAFold[] =
    "AaAaAaCcCcCcCcDd"
    "DdEeEeEeEeEeGgGg"
    "GgGgHhHhIiIiIiIi"
    "IiIiJjKkkLlLlLlL"
    "lLlNnNnNnnNnOoOo"
    "OoOoRrRrRrSsSsSs"
    "SsTtTtTtUuUuUuUu"
    "UuUuWwYyYZzZzZzs";

// Plain-ASCII stand-ins for characters the font doesn't draw. Returns nullptr
// when there's no sensible fallback. `buf` holds single-letter results.
const char* foldToAscii(uint32_t cp, char (&buf)[2]) {
  if (cp >= 0xC0 && cp <= 0xFF) return kLatin1Fold[cp - 0xC0];
  switch (cp) {
    case 0x132: return "IJ";
    case 0x133: return "ij";
    case 0x152: return "OE";
    case 0x153: return "oe";
    case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0x2032: case 0xB4: case 0x2BC:
      return "'";
    case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0x2033: case 0xAB: case 0xBB:
      return "\"";
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: case 0x2212:
      return "-";
    case 0x2026: return "...";
    case 0x2044: return "/";
    case 0xA1: return "!";
    case 0xBF: return "?";
    case 0xA9: return "(c)";
    case 0xAE: return "(R)";
    case 0x2122: return "TM";
    case 0xB9: return "1";
    case 0xB2: return "2";
    case 0xB3: return "3";
    case 0xBC: return "1/4";
    case 0xBD: return "1/2";
    case 0xBE: return "3/4";
    default: break;
  }
  if (cp >= 0x100 && cp <= 0x17F) {
    buf[0] = kLatinExtAFold[cp - 0x100];
    buf[1] = '\0';
    return buf;
  }
  return nullptr;
}

}  // namespace

uint32_t decodeUtf8(std::string_view s, size_t& i) {
  const uint8_t b0 = static_cast<uint8_t>(s[i]);
  if (b0 < 0x80) {
    ++i;
    return b0;
  }
  size_t len;
  uint32_t cp;
  uint32_t min;
  if ((b0 & 0xE0) == 0xC0) {
    len = 2;
    cp = b0 & 0x1F;
    min = 0x80;
  } else if ((b0 & 0xF0) == 0xE0) {
    len = 3;
    cp = b0 & 0x0F;
    min = 0x800;
  } else if ((b0 & 0xF8) == 0xF0) {
    len = 4;
    cp = b0 & 0x07;
    min = 0x10000;
  } else {
    ++i;
    return kReplacementChar;
  }
  if (i + len > s.size()) {
    ++i;
    return kReplacementChar;
  }
  for (size_t k = 1; k < len; ++k) {
    const uint8_t b = static_cast<uint8_t>(s[i + k]);
    if ((b & 0xC0) != 0x80) {
      ++i;
      return kReplacementChar;
    }
    cp = (cp << 6) | (b & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    ++i;
    return kReplacementChar;
  }
  i += len;
  return cp;
}

void TextLine::set(std::string_view text) {
  items_.clear();
  width_ = 0;
  pendingSpace_ = false;
  colored_ = false;
  color_ = Rgb();

  size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '{') {
      if (i + 1 < text.size() && text[i + 1] == '{') {
        emitGlyph('{');
        i += 2;
        continue;
      }
      const size_t close = text.find('}', i + 1);
      if (close != std::string_view::npos && close - i - 1 <= kMaxMarkup) {
        const std::string_view inner = text.substr(i + 1, close - i - 1);
        Rgb parsed;
        if (inner.empty()) {
          colored_ = false;
          i = close + 1;
          continue;
        }
        if (parseColor(inner, parsed)) {
          colored_ = true;
          color_ = parsed;
          i = close + 1;
          continue;
        }
      }
    } else if (c == ':') {
      const size_t close = text.find(':', i + 1);
      if (close != std::string_view::npos && close > i + 1 && close - i - 1 <= kMaxMarkup) {
        char name[kMaxMarkup];
        size_t n = 0;
        bool valid = true;
        for (size_t k = i + 1; k < close; ++k) {
          char ch = text[k];
          if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
          if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')) {
            valid = false;
            break;
          }
          name[n++] = ch;
        }
        const int icon = valid ? findIconByName(std::string_view(name, n)) : -1;
        if (icon >= 0) {
          emitIcon(icon);
          i = close + 1;
          continue;
        }
      }
    }

    const uint32_t cp = decodeUtf8(text, i);
    if (isSpace(cp)) {
      if (!items_.empty()) pendingSpace_ = true;
      continue;
    }
    if (isIgnorable(cp)) continue;
    const int icon = findIconByCodepoint(cp);
    if (icon >= 0) {
      emitIcon(icon);
      continue;
    }
    if (findGlyph(cp) != nullptr) {
      emitGlyph(cp);
      continue;
    }
    if (cp == 0xB7 || cp == 0x2219 || cp == 0x25CF) {  // middle dot, bullet operator, ●
      emitGlyph(0x2022);
      continue;
    }
    char buf[2];
    if (const char* ascii = foldToAscii(cp, buf)) {
      for (const char* p = ascii; *p != '\0'; ++p) emitGlyph(static_cast<uint8_t>(*p));
      continue;
    }
    if (isEmojiLike(cp)) continue;
    emitGlyph(kReplacementChar);
  }
  if (!items_.empty()) width_ = items_.back().x + items_.back().width;
}

void TextLine::emitGlyph(uint32_t codepoint) {
  const GlyphInfo* g = findGlyph(codepoint);
  if (g == nullptr) g = findGlyph(kReplacementChar);
  push(ItemKind::Glyph, static_cast<uint16_t>(g - font_data::kGlyphs), g->width);
}

void TextLine::emitIcon(int icon) { push(ItemKind::Icon, static_cast<uint16_t>(icon), iconInfo(icon).width); }

void TextLine::push(ItemKind kind, uint16_t ref, uint8_t width) {
  auto nextX = [this]() {
    return items_.empty() ? 0 : items_.back().x + items_.back().width + kGlyphSpacing;
  };
  if (pendingSpace_ && !items_.empty()) {
    const GlyphInfo* space = findGlyph(' ');
    items_.push_back(TextItem{ItemKind::Glyph, static_cast<uint16_t>(space - font_data::kGlyphs),
                              space->width, false, Rgb(), static_cast<int16_t>(nextX())});
  }
  pendingSpace_ = false;
  items_.push_back(TextItem{kind, ref, width, colored_, color_, static_cast<int16_t>(nextX())});
}

}  // namespace lb
