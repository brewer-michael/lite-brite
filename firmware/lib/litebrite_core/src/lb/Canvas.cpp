#include "lb/Canvas.h"

namespace lb {

Canvas::Canvas(int width, int height)
    : width_(width > 0 ? width : 1),
      height_(height > 0 ? height : 1),
      pixels_(static_cast<size_t>(width_) * static_cast<size_t>(height_)) {}

void Canvas::clear(Rgb color) {
  for (auto& p : pixels_) p = color;
}

void Canvas::set(int x, int y, Rgb color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  pixels_[static_cast<size_t>(y) * width_ + x] = color;
}

Rgb Canvas::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return colors::kBlack;
  return pixels_[static_cast<size_t>(y) * width_ + x];
}

uint16_t MatrixLayout::index(uint16_t x, uint16_t y) const {
  if (flipX) x = static_cast<uint16_t>(width - 1 - x);
  if (flipY) y = static_cast<uint16_t>(height - 1 - y);
  if (columnMajor) {
    const uint16_t row = (serpentine && (x & 1)) ? static_cast<uint16_t>(height - 1 - y) : y;
    return static_cast<uint16_t>(x * height + row);
  }
  const uint16_t col = (serpentine && (y & 1)) ? static_cast<uint16_t>(width - 1 - x) : x;
  return static_cast<uint16_t>(y * width + col);
}

}  // namespace lb
