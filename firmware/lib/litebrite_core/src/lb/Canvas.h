#pragma once

#include <cstdint>
#include <vector>

#include "lb/Color.h"

namespace lb {

// A logical framebuffer: (0,0) is the top-left pixel as a viewer sees it.
class Canvas {
 public:
  Canvas(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

  void clear(Rgb color = colors::kBlack);
  void set(int x, int y, Rgb color);  // ignores out-of-range coordinates
  Rgb get(int x, int y) const;        // black when out of range
  const Rgb* data() const { return pixels_.data(); }
  size_t size() const { return pixels_.size(); }

 private:
  int width_;
  int height_;
  std::vector<Rgb> pixels_;
};

// Maps logical (x, y) to the LED's position on the data chain.
//
// Common 8x32 flexible panels run in vertical columns that zig-zag
// ("column-major serpentine"). If text comes out mirrored or upside down,
// flip X/Y; if it's scrambled, toggle column-major or serpentine. The
// firmware's "wiring test" command shows the chain order.
struct MatrixLayout {
  uint16_t width = 32;
  uint16_t height = 8;
  bool columnMajor = true;
  bool serpentine = true;
  bool flipX = false;
  bool flipY = false;

  uint16_t count() const { return static_cast<uint16_t>(width * height); }
  uint16_t index(uint16_t x, uint16_t y) const;
};

}  // namespace lb
