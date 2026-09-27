#include <unity.h>

#include "lb/Battery.h"
#include "lb/Policy.h"
#include "lb/Settings.h"

using namespace lb;

void setUp() {}
void tearDown() {}

// ---- Battery ---------------------------------------------------------------

void test_battery_percent_curve() {
  TEST_ASSERT_EQUAL_UINT8(100, batteryPercent(4250));
  TEST_ASSERT_EQUAL_UINT8(100, batteryPercent(4200));
  TEST_ASSERT_EQUAL_UINT8(50, batteryPercent(3840));
  TEST_ASSERT_EQUAL_UINT8(0, batteryPercent(3270));
  TEST_ASSERT_EQUAL_UINT8(0, batteryPercent(3000));
  TEST_ASSERT_EQUAL_UINT8(3, batteryPercent(3474));  // interpolated between 0% and 5%
  uint8_t last = 0;
  for (uint16_t mv = 3200; mv <= 4250; mv += 5) {
    const uint8_t p = batteryPercent(mv);
    TEST_ASSERT_TRUE(p >= last);  // monotonic
    last = p;
  }
}

void test_battery_levels() {
  const BatteryThresholds t;
  TEST_ASSERT_EQUAL(BatteryLevel::Unknown, batteryLevel(0, t));
  TEST_ASSERT_EQUAL(BatteryLevel::Ok, batteryLevel(3900, t));
  TEST_ASSERT_EQUAL(BatteryLevel::Low, batteryLevel(3650, t));
  TEST_ASSERT_EQUAL(BatteryLevel::Critical, batteryLevel(3450, t));
  TEST_ASSERT_EQUAL(BatteryLevel::Empty, batteryLevel(3300, t));
}

void test_battery_hysteresis() {
  const BatteryThresholds t;
  // Dropping is immediate...
  TEST_ASSERT_EQUAL(BatteryLevel::Low, batteryLevel(3690, t, BatteryLevel::Ok));
  // ...but a reading just over the line doesn't recover.
  TEST_ASSERT_EQUAL(BatteryLevel::Low, batteryLevel(3720, t, BatteryLevel::Low));
  TEST_ASSERT_EQUAL(BatteryLevel::Ok, batteryLevel(3760, t, BatteryLevel::Low));
  TEST_ASSERT_EQUAL(BatteryLevel::Critical, batteryLevel(3520, t, BatteryLevel::Critical));
  TEST_ASSERT_EQUAL(BatteryLevel::Low, batteryLevel(3560, t, BatteryLevel::Critical));
  TEST_ASSERT_EQUAL(BatteryLevel::Ok, batteryLevel(4100, t, BatteryLevel::Empty));  // charged up
}

void test_led_budget_tapers() {
  const BatteryThresholds t;
  TEST_ASSERT_EQUAL_UINT32(1200, ledBudgetMa(0, 1200, t));
  TEST_ASSERT_EQUAL_UINT32(1200, ledBudgetMa(4000, 1200, t));
  TEST_ASSERT_EQUAL_UINT32(480, ledBudgetMa(3500, 1200, t));
  TEST_ASSERT_EQUAL_UINT32(840, ledBudgetMa(3650, 1200, t));
  TEST_ASSERT_EQUAL_UINT32(0, ledBudgetMa(3499, 1200, t));
}

// ---- Settings --------------------------------------------------------------

void test_settings_apply_and_clamp() {
  Settings s;
  SettingId id;
  TEST_ASSERT_TRUE(findSetting("brightness", id));
  TEST_ASSERT_TRUE(applySetting(s, id, "75"));
  TEST_ASSERT_EQUAL_UINT8(75, s.brightness);
  TEST_ASSERT_TRUE(applySetting(s, id, "75.6"));  // HA number entities may send floats
  TEST_ASSERT_EQUAL_UINT8(76, s.brightness);
  TEST_ASSERT_TRUE(applySetting(s, id, "250"));
  TEST_ASSERT_EQUAL_UINT8(100, s.brightness);
  TEST_ASSERT_TRUE(applySetting(s, id, "1e300"));
  TEST_ASSERT_EQUAL_UINT8(100, s.brightness);
  TEST_ASSERT_FALSE(applySetting(s, id, "bright"));
  TEST_ASSERT_FALSE(applySetting(s, id, ""));
  TEST_ASSERT_EQUAL_UINT8(100, s.brightness);

  TEST_ASSERT_TRUE(findSetting("wake_interval", id));
  TEST_ASSERT_TRUE(applySetting(s, id, "2"));
  TEST_ASSERT_EQUAL_UINT16(5, s.wakeInterval);
  TEST_ASSERT_EQUAL_STRING("5", settingValue(s, id).c_str());

  TEST_ASSERT_TRUE(findSetting("stay_awake", id));
  TEST_ASSERT_TRUE(isSwitch(id));
  TEST_ASSERT_TRUE(applySetting(s, id, "ON"));
  TEST_ASSERT_TRUE(s.stayAwake);
  TEST_ASSERT_EQUAL_STRING("ON", settingValue(s, id).c_str());
  TEST_ASSERT_TRUE(applySetting(s, id, "off"));
  TEST_ASSERT_FALSE(s.stayAwake);
  TEST_ASSERT_FALSE(applySetting(s, id, "maybe"));

  TEST_ASSERT_FALSE(findSetting("volume", id));
}

void test_setting_table_is_consistent() {
  for (size_t i = 0; i < kSettingCount; ++i) {
    const SettingInfo& info = settingInfoAt(i);
    TEST_ASSERT_EQUAL(static_cast<int>(i), static_cast<int>(info.id));
    SettingId found;
    TEST_ASSERT_TRUE(findSetting(info.key, found));
    TEST_ASSERT_EQUAL(info.id, found);
    // Defaults are inside their own ranges.
    Settings defaults;
    Settings copy = defaults;
    TEST_ASSERT_TRUE(applySetting(copy, info.id, settingValue(defaults, info.id)));
    TEST_ASSERT_EQUAL_STRING(settingValue(defaults, info.id).c_str(), settingValue(copy, info.id).c_str());
  }
}

// ---- Policy ----------------------------------------------------------------

static AwakeState motionWake() {
  AwakeState s;
  s.reason = WakeReason::Motion;
  s.motionSeen = true;
  s.lastMotionMs = 0;
  s.lingerS = 20;
  return s;
}

void test_timer_wake_sleeps_once_idle() {
  AwakeState s;
  s.reason = WakeReason::Timer;
  s.nowMs = 3000;
  s.busy = true;
  TEST_ASSERT_TRUE(keepAwake(s));
  s.busy = false;
  TEST_ASSERT_FALSE(keepAwake(s));
}

void test_motion_wake_lingers() {
  AwakeState s = motionWake();
  s.nowMs = 19999;
  TEST_ASSERT_TRUE(keepAwake(s));
  s.nowMs = 20000;
  TEST_ASSERT_FALSE(keepAwake(s));
  s.lastMotionMs = 15000;  // more motion extends it
  TEST_ASSERT_TRUE(keepAwake(s));
  s.nowMs = 60000;
  s.lastActivityMs = 50000;  // a show that just ended extends it too
  TEST_ASSERT_TRUE(keepAwake(s));
  s.lingerS = 0;
  TEST_ASSERT_FALSE(keepAwake(s));
}

void test_stuck_motion_sensor_cannot_keep_it_awake() {
  AwakeState s = motionWake();
  s.nowMs = kMaxLingerAwakeMs - 1;
  s.lastMotionMs = s.nowMs;  // "motion" right now, as a stuck sensor reports
  TEST_ASSERT_TRUE(keepAwake(s));
  s.nowMs = kMaxLingerAwakeMs;
  s.lastMotionMs = s.nowMs;
  TEST_ASSERT_FALSE(keepAwake(s));
  s.busy = true;  // but a message on screen always finishes
  TEST_ASSERT_TRUE(keepAwake(s));
}

void test_timestamps_just_after_now_count_as_now() {
  AwakeState s = motionWake();
  s.nowMs = 50000;
  s.lastMotionMs = 10000;
  s.lastActivityMs = 50002;  // a show ended after `now` was read
  TEST_ASSERT_TRUE(keepAwake(s));
  s.lastActivityMs = 0;
  s.lastMotionMs = 50001;
  TEST_ASSERT_TRUE(someonePresent(s));
  TEST_ASSERT_TRUE(keepAwake(s));
  // ...but long uptimes (a USB-powered sign) still see old motion as old.
  s.nowMs = 26u * 24 * 3600 * 1000;  // 26 days
  s.lastMotionMs = 1000;
  TEST_ASSERT_FALSE(someonePresent(s));
}

void test_power_on_stays_up_for_setup() {
  AwakeState s;
  s.reason = WakeReason::PowerOn;
  s.nowMs = kPowerOnAwakeMs - 1;
  TEST_ASSERT_TRUE(keepAwake(s));
  s.nowMs = kPowerOnAwakeMs;
  TEST_ASSERT_FALSE(keepAwake(s));
}

void test_always_on_and_maintenance() {
  AwakeState s;
  s.reason = WakeReason::Timer;
  s.nowMs = 999999;
  s.alwaysOn = true;
  TEST_ASSERT_TRUE(keepAwake(s));
  s.alwaysOn = false;
  s.stayAwake = true;
  TEST_ASSERT_TRUE(keepAwake(s));
}

void test_presence_and_may_show() {
  AwakeState s = motionWake();
  s.nowMs = kPresenceWindowMs - 1;
  TEST_ASSERT_TRUE(someonePresent(s));
  s.nowMs = kPresenceWindowMs;
  TEST_ASSERT_FALSE(someonePresent(s));
  AwakeState timer;
  timer.reason = WakeReason::Timer;
  TEST_ASSERT_FALSE(someonePresent(timer));

  Message waits;
  Message now;
  now.when = ShowWhen::Now;
  TEST_ASSERT_FALSE(mayShow(waits, false, true));
  TEST_ASSERT_TRUE(mayShow(waits, true, true));
  TEST_ASSERT_TRUE(mayShow(waits, false, false));  // no motion sensor fitted
  TEST_ASSERT_TRUE(mayShow(now, false, true));
}

void test_sleep_plans() {
  SleepPlan p = planSleep(60, BatteryLevel::Ok, true);
  TEST_ASSERT_EQUAL_UINT32(3600, p.timerSeconds);
  TEST_ASSERT_TRUE(p.armMotion);
  p = planSleep(60, BatteryLevel::Low, false);
  TEST_ASSERT_FALSE(p.armMotion);
  p = planSleep(30, BatteryLevel::Critical, true);
  TEST_ASSERT_EQUAL_UINT32(4 * 3600, p.timerSeconds);
  TEST_ASSERT_FALSE(p.armMotion);
  p = planSleep(120, BatteryLevel::Critical, true);
  TEST_ASSERT_EQUAL_UINT32(8 * 3600, p.timerSeconds);
  p = planSleep(60, BatteryLevel::Empty, true);
  TEST_ASSERT_EQUAL_UINT32(12 * 3600, p.timerSeconds);
  TEST_ASSERT_FALSE(p.armMotion);
  TEST_ASSERT_EQUAL_STRING("motion", wakeReasonName(WakeReason::Motion));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_battery_percent_curve);
  RUN_TEST(test_battery_levels);
  RUN_TEST(test_battery_hysteresis);
  RUN_TEST(test_led_budget_tapers);
  RUN_TEST(test_settings_apply_and_clamp);
  RUN_TEST(test_setting_table_is_consistent);
  RUN_TEST(test_timer_wake_sleeps_once_idle);
  RUN_TEST(test_motion_wake_lingers);
  RUN_TEST(test_stuck_motion_sensor_cannot_keep_it_awake);
  RUN_TEST(test_timestamps_just_after_now_count_as_now);
  RUN_TEST(test_power_on_stays_up_for_setup);
  RUN_TEST(test_always_on_and_maintenance);
  RUN_TEST(test_presence_and_may_show);
  RUN_TEST(test_sleep_plans);
  return UNITY_END();
}
