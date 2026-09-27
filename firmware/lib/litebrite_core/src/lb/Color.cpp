#include "lb/Color.h"

#include <cstddef>

namespace lb {

namespace {

struct NamedColor {
  const char* name;  // normalized: lowercase, no spaces/dashes/underscores
  Rgb color;
};

// sRGB values. The output stage applies gamma, so these read on the LEDs the
// way they look on a screen.
constexpr NamedColor kNamed[] = {
    {"red", {255, 0, 0}},
    {"orange", {255, 165, 0}},
    {"amber", {255, 191, 0}},
    {"gold", {255, 215, 0}},
    {"yellow", {255, 235, 0}},
    {"lime", {160, 255, 0}},
    {"green", {0, 255, 0}},
    {"mint", {60, 255, 160}},
    {"teal", {0, 200, 170}},
    {"cyan", {0, 255, 255}},
    {"aqua", {0, 255, 255}},
    {"sky", {80, 190, 255}},
    {"skyblue", {80, 190, 255}},
    {"blue", {0, 90, 255}},
    {"purple", {170, 0, 255}},
    {"violet", {200, 90, 255}},
    {"lavender", {190, 150, 255}},
    {"magenta", {255, 0, 255}},
    {"fuchsia", {255, 0, 255}},
    {"pink", {255, 105, 180}},
    {"coral", {255, 127, 80}},
    {"salmon", {250, 128, 114}},
    {"brown", {150, 75, 0}},
    {"white", {255, 255, 255}},
    {"warmwhite", {255, 224, 180}},
    {"grey", {128, 128, 128}},
    {"gray", {128, 128, 128}},
    {"black", {0, 0, 0}},
    {"off", {0, 0, 0}},
};

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = lower(c);
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Lowercases and drops ' ', '-', '_' into buf. Returns false if it won't fit.
bool normalize(std::string_view text, char* buf, size_t cap, size_t& len) {
  len = 0;
  for (char c : text) {
    if (c == ' ' || c == '-' || c == '_') continue;
    if (len + 1 >= cap) return false;
    buf[len++] = lower(c);
  }
  buf[len] = '\0';
  return true;
}

bool parseHex(std::string_view h, Rgb& out) {
  int v[6];
  if (h.size() != 3 && h.size() != 6) return false;
  for (size_t i = 0; i < h.size(); ++i) {
    v[i] = hexValue(h[i]);
    if (v[i] < 0) return false;
  }
  if (h.size() == 3) {
    out = Rgb(static_cast<uint8_t>(v[0] * 17), static_cast<uint8_t>(v[1] * 17),
              static_cast<uint8_t>(v[2] * 17));
  } else {
    out = Rgb(static_cast<uint8_t>(v[0] * 16 + v[1]), static_cast<uint8_t>(v[2] * 16 + v[3]),
              static_cast<uint8_t>(v[4] * 16 + v[5]));
  }
  return true;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  return s;
}

}  // namespace

const char* const kColorNames[] = {
    "red",  "orange", "amber",    "gold",    "yellow", "lime",  "green",      "mint",
    "teal", "cyan",   "sky blue", "blue",    "purple", "violet", "lavender",  "magenta",
    "pink", "coral",  "salmon",   "brown",   "white",  "warm white", "rainbow", nullptr,
};

Rgb scale(Rgb c, uint8_t amount) {
  auto ch = [amount](uint8_t v) {
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * amount + 127) / 255);
  };
  return Rgb(ch(c.r), ch(c.g), ch(c.b));
}

Rgb hsv(int hueDegrees, uint8_t sat, uint8_t val) {
  int h = hueDegrees % 360;
  if (h < 0) h += 360;
  const int region = h / 60;
  const int rem = (h % 60) * 255 / 60;  // 0..255 within the region
  const int p = val * (255 - sat) / 255;
  const int q = val * (255 - (sat * rem) / 255) / 255;
  const int t = val * (255 - (sat * (255 - rem)) / 255) / 255;
  auto u8 = [](int v) { return static_cast<uint8_t>(v); };
  switch (region) {
    case 0:
      return Rgb(val, u8(t), u8(p));
    case 1:
      return Rgb(u8(q), val, u8(p));
    case 2:
      return Rgb(u8(p), val, u8(t));
    case 3:
      return Rgb(u8(p), u8(q), val);
    case 4:
      return Rgb(u8(t), u8(p), val);
    default:
      return Rgb(val, u8(p), u8(q));
  }
}

bool parseColor(std::string_view text, Rgb& out) {
  text = trim(text);
  if (text.empty()) return false;
  if (text.front() == '#') return parseHex(text.substr(1), out);

  char buf[24];
  size_t len = 0;
  if (!normalize(text, buf, sizeof(buf), len) || len == 0) return false;
  const std::string_view name(buf, len);
  for (const auto& nc : kNamed) {
    if (name == nc.name) {
      out = nc.color;
      return true;
    }
  }
  // Bare hex without '#', e.g. "ff8800" (handy inside {..} markup).
  if (len == 6) return parseHex(name, out);
  return false;
}

bool isRainbow(std::string_view text) {
  char buf[16];
  size_t len = 0;
  if (!normalize(trim(text), buf, sizeof(buf), len)) return false;
  const std::string_view name(buf, len);
  return name == "rainbow" || name == "rainbows";
}

}  // namespace lb
