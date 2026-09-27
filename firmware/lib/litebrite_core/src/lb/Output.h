#pragma once

#include <cstddef>
#include <cstdint>

#include "lb/Canvas.h"

namespace lb {

// Byte order the LED chips expect on the wire. WS2812B is GRB.
enum class ColorOrder : uint8_t { RGB, RBG, GRB, GBR, BRG, BGR };

// How much current addressable LEDs draw, for the power limiter.
struct LedPowerModel {
  uint16_t maPerChannel = 16;   // one color channel at full PWM
  uint16_t idleUaPerLed = 700;  // quiescent draw of each LED's driver chip, even when dark
};

// Maps a perceptual brightness percentage (what a person means by "50%") to a
// PWM scale factor. 0 stays 0; anything above 0 is at least 1.
uint8_t brightnessLevel(uint8_t percent);

// Estimated current for a wire buffer, in mA.
uint32_t estimateMilliamps(const uint8_t* wire, size_t ledCount, const LedPowerModel& model);

// Turns a Canvas into bytes for the LED chain: gamma correction, brightness,
// physical pixel order, channel order, and a hard cap on estimated current so
// a white screen can't brown out the battery.
class OutputStage {
 public:
  explicit OutputStage(float gamma = 2.6f);

  // Fills `wire` (3 bytes per LED, layout.count() LEDs) and returns the
  // estimated current in mA after limiting to `budgetMa`.
  uint32_t render(const Canvas& canvas, const MatrixLayout& layout, ColorOrder order, uint8_t level,
                  uint32_t budgetMa, const LedPowerModel& model, uint8_t* wire) const;

  uint8_t gamma(uint8_t v) const { return lut_[v]; }

 private:
  uint8_t lut_[256];
};

}  // namespace lb
