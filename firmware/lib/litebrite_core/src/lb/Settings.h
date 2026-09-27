#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace lb {

// Device settings controlled from Home Assistant. Each one is a retained MQTT
// topic (lite-brite/<device>/config/<key>) backing a number or switch entity,
// so a sleeping sign picks up changes the next time it wakes.
struct Settings {
  uint8_t brightness = 50;     // % (perceptual)
  uint8_t speed = 24;          // scroll speed, px/s
  uint16_t duration = 30;      // default seconds per message
  uint16_t linger = 20;        // seconds to stay awake listening after motion
  uint16_t wakeInterval = 60;  // minutes between battery check-ins
  bool stayAwake = false;      // maintenance mode: stay connected, allow OTA
};

enum class SettingId : uint8_t { Brightness, Speed, Duration, Linger, WakeInterval, StayAwake };
constexpr size_t kSettingCount = 6;

struct SettingInfo {
  SettingId id;
  const char* key;   // topic suffix and discovery component id
  const char* name;  // Home Assistant entity name
  int min;           // numbers only
  int max;
  const char* unit;  // nullptr for none
  const char* icon;  // mdi icon
};

const SettingInfo& settingInfo(SettingId id);
const SettingInfo& settingInfoAt(size_t index);  // 0..kSettingCount-1
bool findSetting(std::string_view key, SettingId& id);
bool isSwitch(SettingId id);

// Applies a payload ("42", "42.0", "ON", "off"...). Numbers are clamped to the
// setting's range. Returns false if the payload isn't valid for the setting.
bool applySetting(Settings& s, SettingId id, std::string_view payload);

// The setting's current value as it's published ("42" or "ON"/"OFF").
std::string settingValue(const Settings& s, SettingId id);

}  // namespace lb
