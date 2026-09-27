#include "lb/Battery.h"

#include <cstddef>

namespace lb {

namespace {

struct CurvePoint {
  uint16_t mv;
  uint8_t percent;
};

// Typical resting voltage vs. state of charge for a 1S LiPo.
constexpr CurvePoint kCurve[] = {
    {3270, 0},  {3610, 5},  {3690, 10}, {3710, 15}, {3730, 20}, {3750, 25}, {3770, 30},
    {3790, 35}, {3800, 40}, {3820, 45}, {3840, 50}, {3850, 55}, {3870, 60}, {3910, 65},
    {3950, 70}, {3980, 75}, {4020, 80}, {4080, 85}, {4110, 90}, {4150, 95}, {4200, 100},
};

int rank(BatteryLevel level) {
  switch (level) {
    case BatteryLevel::Empty: return 0;
    case BatteryLevel::Critical: return 1;
    case BatteryLevel::Low: return 2;
    case BatteryLevel::Ok: return 3;
    case BatteryLevel::Unknown: return 3;
  }
  return 3;
}

BatteryLevel classify(uint16_t mv, const BatteryThresholds& t, uint16_t margin) {
  if (mv < t.emptyMv + margin) return BatteryLevel::Empty;
  if (mv < t.criticalMv + margin) return BatteryLevel::Critical;
  if (mv < t.lowMv + margin) return BatteryLevel::Low;
  return BatteryLevel::Ok;
}

}  // namespace

uint8_t batteryPercent(uint16_t millivolts) {
  constexpr size_t n = sizeof(kCurve) / sizeof(kCurve[0]);
  if (millivolts <= kCurve[0].mv) return 0;
  if (millivolts >= kCurve[n - 1].mv) return 100;
  for (size_t i = 1; i < n; ++i) {
    if (millivolts <= kCurve[i].mv) {
      const CurvePoint& a = kCurve[i - 1];
      const CurvePoint& b = kCurve[i];
      const uint32_t span = b.mv - a.mv;
      const uint32_t into = millivolts - a.mv;
      return static_cast<uint8_t>(a.percent + ((b.percent - a.percent) * into + span / 2) / span);
    }
  }
  return 100;
}

BatteryLevel batteryLevel(uint16_t millivolts, const BatteryThresholds& t, BatteryLevel previous) {
  if (millivolts == 0) return BatteryLevel::Unknown;
  const BatteryLevel plain = classify(millivolts, t, 0);
  if (previous == BatteryLevel::Unknown || rank(plain) <= rank(previous)) return plain;
  // Improving: only move up once the reading clears the threshold by the margin.
  const BatteryLevel strict = classify(millivolts, t, t.hysteresisMv);
  return rank(strict) > rank(previous) ? strict : previous;
}

const char* batteryLevelName(BatteryLevel level) {
  switch (level) {
    case BatteryLevel::Unknown: return "unknown";
    case BatteryLevel::Ok: return "ok";
    case BatteryLevel::Low: return "low";
    case BatteryLevel::Critical: return "critical";
    case BatteryLevel::Empty: return "empty";
  }
  return "unknown";
}

uint32_t ledBudgetMa(uint16_t millivolts, uint32_t maxMa, const BatteryThresholds& t) {
  constexpr uint16_t kFullMv = 3800;
  if (millivolts == 0 || millivolts >= kFullMv) return maxMa;
  if (millivolts < t.criticalMv) return 0;
  // Linear from 40% at the critical threshold to 100% at kFullMv.
  const uint32_t span = kFullMv - t.criticalMv;
  const uint32_t into = millivolts - t.criticalMv;
  return maxMa * (40 + 60 * into / span) / 100;
}

}  // namespace lb
