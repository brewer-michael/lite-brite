#include "lb/Policy.h"

#include <algorithm>

namespace lb {

const char* wakeReasonName(WakeReason reason) {
  switch (reason) {
    case WakeReason::PowerOn: return "power_on";
    case WakeReason::Motion: return "motion";
    case WakeReason::Timer: return "timer";
    case WakeReason::Other: return "other";
  }
  return "other";
}

namespace {

// Milliseconds since `then`. A `then` up to a minute after `now` (read from
// millis() later in the same loop) counts as "just now", not 49 days ago.
uint32_t elapsed(uint32_t now, uint32_t then) {
  const uint32_t d = now - then;
  return d > 0xFFFFFFFFu - 60000u ? 0 : d;
}

}  // namespace

bool someonePresent(const AwakeState& s) {
  return s.motionSeen && elapsed(s.nowMs, s.lastMotionMs) < kPresenceWindowMs;
}

bool keepAwake(const AwakeState& s) {
  if (s.alwaysOn || s.stayAwake || s.busy) return true;
  if (s.reason == WakeReason::PowerOn && s.nowMs < kPowerOnAwakeMs) return true;
  if (!s.motionSeen || s.nowMs >= kMaxLingerAwakeMs) return false;
  const uint32_t since = std::min(elapsed(s.nowMs, s.lastMotionMs), elapsed(s.nowMs, s.lastActivityMs));
  return since < static_cast<uint32_t>(s.lingerS) * 1000u;
}

bool mayShow(const Message& msg, bool present, bool hasMotionSensor) {
  return msg.when == ShowWhen::Now || !hasMotionSensor || present;
}

SleepPlan planSleep(uint16_t wakeIntervalMin, BatteryLevel battery, bool hasMotionSensor) {
  const uint32_t interval = static_cast<uint32_t>(std::max<uint16_t>(wakeIntervalMin, 1)) * 60u;
  switch (battery) {
    case BatteryLevel::Empty:
      return {12u * 3600u, false};
    case BatteryLevel::Critical:
      return {std::max<uint32_t>(interval * 4u, 4u * 3600u), false};
    default:
      return {interval, hasMotionSensor};
  }
}

}  // namespace lb
