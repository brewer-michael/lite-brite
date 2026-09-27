#include "lb/Output.h"

#include <cmath>
#include <cstring>

namespace lb {

namespace {

void placeChannels(uint8_t* px, ColorOrder order, uint8_t r, uint8_t g, uint8_t b) {
  switch (order) {
    case ColorOrder::RGB: px[0] = r; px[1] = g; px[2] = b; break;
    case ColorOrder::RBG: px[0] = r; px[1] = b; px[2] = g; break;
    case ColorOrder::GRB: px[0] = g; px[1] = r; px[2] = b; break;
    case ColorOrder::GBR: px[0] = g; px[1] = b; px[2] = r; break;
    case ColorOrder::BRG: px[0] = b; px[1] = r; px[2] = g; break;
    case ColorOrder::BGR: px[0] = b; px[1] = g; px[2] = r; break;
  }
}

}  // namespace

uint8_t brightnessLevel(uint8_t percent) {
  if (percent == 0) return 0;
  if (percent >= 100) return 255;
  const double v = 255.0 * std::pow(percent / 100.0, 2.2);
  const long level = std::lround(v);
  return static_cast<uint8_t>(level < 1 ? 1 : level);
}

uint32_t estimateMilliamps(const uint8_t* wire, size_t ledCount, const LedPowerModel& model) {
  uint32_t sum = 0;
  for (size_t i = 0; i < ledCount * 3; ++i) sum += wire[i];
  const uint32_t idle = static_cast<uint32_t>(ledCount) * model.idleUaPerLed / 1000;
  return idle + (sum * model.maPerChannel + 127) / 255;
}

OutputStage::OutputStage(float gamma) {
  for (int i = 0; i < 256; ++i) {
    lut_[i] = static_cast<uint8_t>(std::lround(255.0 * std::pow(i / 255.0, gamma)));
  }
}

uint32_t OutputStage::render(const Canvas& canvas, const MatrixLayout& layout, ColorOrder order,
                             uint8_t level, uint32_t budgetMa, const LedPowerModel& model,
                             uint8_t* wire) const {
  const size_t leds = layout.count();
  std::memset(wire, 0, leds * 3);
  auto dim = [this, level](uint8_t v) {
    return static_cast<uint8_t>((static_cast<uint16_t>(lut_[v]) * level + 127) / 255);
  };
  const int w = canvas.width() < layout.width ? canvas.width() : layout.width;
  const int h = canvas.height() < layout.height ? canvas.height() : layout.height;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Rgb c = canvas.get(x, y);
      if (c.isBlack()) continue;
      const uint16_t i = layout.index(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
      placeChannels(&wire[i * 3], order, dim(c.r), dim(c.g), dim(c.b));
    }
  }

  const uint32_t idle = static_cast<uint32_t>(leds) * model.idleUaPerLed / 1000;
  uint32_t total = estimateMilliamps(wire, leds, model);
  if (total <= budgetMa) return total;
  if (budgetMa <= idle) {  // no headroom at all: go dark
    std::memset(wire, 0, leds * 3);
    return idle;
  }
  // Scale every channel down by the same factor (16.16 fixed point).
  const uint32_t active = total - idle;
  const uint32_t factor = static_cast<uint32_t>((uint64_t(budgetMa - idle) << 16) / active);
  for (size_t i = 0; i < leds * 3; ++i) wire[i] = static_cast<uint8_t>((wire[i] * factor) >> 16);
  return estimateMilliamps(wire, leds, model);
}

}  // namespace lb
