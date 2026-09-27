#pragma once

#include <cstdint>

namespace lb {

enum class BatteryLevel : uint8_t {
  Unknown,   // no battery sensing
  Ok,
  Low,       // show a "charge me" hint after messages, dim the LEDs
  Critical,  // don't light the LEDs; stop waking on motion; report and sleep
  Empty,     // protect the cell: check in rarely
};

struct BatteryThresholds {
  uint16_t lowMv = 3700;
  uint16_t criticalMv = 3500;
  uint16_t emptyMv = 3350;
  uint16_t hysteresisMv = 50;  // needed to climb back to a better level
};

// State of charge for a resting single-cell Li-ion/LiPo (piecewise-linear).
uint8_t batteryPercent(uint16_t millivolts);

// Classifies a resting voltage. `previous` adds hysteresis so a reading that
// hovers around a threshold doesn't flip the level on every wake.
BatteryLevel batteryLevel(uint16_t millivolts, const BatteryThresholds& t,
                          BatteryLevel previous = BatteryLevel::Unknown);

const char* batteryLevelName(BatteryLevel level);

// Current the LEDs may draw at this battery voltage: the full budget down to
// 3.8 V, tapering to 40 % at the critical threshold, zero below it. A voltage
// of 0 means "unknown" and allows the full budget.
uint32_t ledBudgetMa(uint16_t millivolts, uint32_t maxMa, const BatteryThresholds& t);

}  // namespace lb
