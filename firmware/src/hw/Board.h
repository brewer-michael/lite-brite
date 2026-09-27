#pragma once

#include <cstdint>

#include "lb/Policy.h"

// Board-level I/O: wake reason, motion sensor, battery and USB sensing, and
// deep sleep. Pins come from include/config.h.
namespace board {

lb::WakeReason wakeReason();

// Configures sensor inputs. Call early in setup().
void begin();

bool hasMotionSensor();
bool motionNow();  // the PIR output is active right now

bool hasBatterySense();
uint16_t batteryMillivolts();  // averaged; 0 if there's no battery sense pin

bool hasUsbSense();
bool usbPower();  // false if there's no VBUS sense pin

// Seconds since an arbitrary epoch that keeps counting through deep sleep
// (it becomes real unix time once NTP has set the clock).
int64_t rtcSeconds();

// Enters deep sleep. Wakes after `seconds` (0 = no timer) and, if armMotion,
// when the PIR output goes active. Never returns.
[[noreturn]] void deepSleep(uint32_t seconds, bool armMotion);

}  // namespace board
