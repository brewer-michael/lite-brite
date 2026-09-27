#pragma once

#include <cstdint>

#include "lb/Battery.h"
#include "lb/Message.h"

namespace lb {

// Battery-life decisions: when to stay awake, when a message may be shown,
// and how to sleep. Pure functions so they can be unit-tested.

enum class WakeReason : uint8_t { PowerOn, Motion, Timer, Other };
const char* wakeReasonName(WakeReason reason);

// Motion within this window means someone is in front of the sign.
constexpr uint32_t kPresenceWindowMs = 30000;
// After power-on (fresh battery, reset button) stay up this long so setup
// changes made in Home Assistant land right away.
constexpr uint32_t kPowerOnAwakeMs = 60000;

struct AwakeState {
  WakeReason reason = WakeReason::PowerOn;
  uint32_t nowMs = 0;           // since boot
  bool motionSeen = false;      // any motion during this wake
  uint32_t lastMotionMs = 0;    // valid when motionSeen
  uint32_t lastActivityMs = 0;  // when the last show ended (0 = none)
  bool busy = false;            // connecting, syncing, showing, updating
  bool alwaysOn = false;        // USB power or a mains-powered build
  bool stayAwake = false;       // maintenance switch in Home Assistant
  uint16_t lingerS = 20;        // listen this long after motion or a show
};

// Someone moved in front of the sign recently.
bool someonePresent(const AwakeState& s);

// Whether to stay awake now. Otherwise the sign should go to deep sleep.
bool keepAwake(const AwakeState& s);

// Messages wait for someone to be there, unless they ask to show now or the
// sign has no motion sensor.
bool mayShow(const Message& msg, bool present, bool hasMotionSensor);

struct SleepPlan {
  uint32_t timerSeconds;  // wake for a check-in after this long
  bool armMotion;         // also wake when the motion sensor fires
};

SleepPlan planSleep(uint16_t wakeIntervalMin, BatteryLevel battery, bool hasMotionSensor);

}  // namespace lb
