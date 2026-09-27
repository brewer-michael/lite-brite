#pragma once

#include <cstdint>
#include <string_view>

namespace lb {

// An sRGB color as the user specified it. Gamma correction for the LEDs
// happens later, in the output stage, so colors here look the way they do on
// a screen (and in the simulator).
struct Rgb {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  constexpr Rgb() = default;
  constexpr Rgb(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

  constexpr bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
  constexpr bool operator!=(const Rgb& o) const { return !(*this == o); }
  constexpr bool isBlack() const { return (r | g | b) == 0; }
};

namespace colors {
constexpr Rgb kBlack{0, 0, 0};
constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kRed{255, 0, 0};
constexpr Rgb kOrange{255, 140, 0};
constexpr Rgb kGreen{0, 220, 0};
}  // namespace colors

// Multiplies every channel by amount/255 (rounded).
Rgb scale(Rgb c, uint8_t amount);

// Hue in degrees (any value, wraps), saturation and value 0-255.
Rgb hsv(int hueDegrees, uint8_t sat, uint8_t val);

// Parses a color name ("orange", "warm white", "warm_white", "Sky-Blue"),
// "#RRGGBB", "RRGGBB" or "#RGB". Case-insensitive; spaces, '-' and '_' in
// names are ignored. Returns false if the text isn't a color.
bool parseColor(std::string_view text, Rgb& out);

// True for "rainbow" (and "rainbows"), which is a color *mode*, not a color.
bool isRainbow(std::string_view text);

// The color choices offered to people (Home Assistant selectors, docs),
// including the "rainbow" mode. Terminated by a nullptr entry.
extern const char* const kColorNames[];

}  // namespace lb
