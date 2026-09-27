#include "lb/Settings.h"

#include <cmath>
#include <cstdlib>

namespace lb {

namespace {

constexpr SettingInfo kSettings[kSettingCount] = {
    {SettingId::Brightness, "brightness", "Brightness", 1, 100, "%", "mdi:brightness-6"},
    {SettingId::Speed, "speed", "Scroll speed", 5, 60, "px/s", "mdi:speedometer"},
    {SettingId::Duration, "duration", "Message duration", 5, 300, "s", "mdi:timer-outline"},
    {SettingId::Linger, "linger", "Listen after motion", 0, 120, "s", "mdi:ear-hearing"},
    {SettingId::WakeInterval, "wake_interval", "Check-in interval", 5, 1440, "min", "mdi:timer-sync-outline"},
    {SettingId::StayAwake, "stay_awake", "Stay awake", 0, 1, nullptr, "mdi:sleep-off"},
};

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return s;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char c = a[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != b[i]) return false;
  }
  return true;
}

bool parseNumber(std::string_view text, long& out) {
  text = trim(text);
  if (text.empty() || text.size() > 20) return false;
  char buf[24];
  text.copy(buf, text.size());
  buf[text.size()] = '\0';
  char* end = nullptr;
  const double d = std::strtod(buf, &end);
  if (end == buf || *end != '\0' || !std::isfinite(d)) return false;
  out = std::lround(d < -1e9 ? -1e9 : (d > 1e9 ? 1e9 : d));  // callers clamp further
  return true;
}

}  // namespace

const SettingInfo& settingInfo(SettingId id) { return kSettings[static_cast<size_t>(id)]; }

const SettingInfo& settingInfoAt(size_t index) { return kSettings[index]; }

bool findSetting(std::string_view key, SettingId& id) {
  for (const auto& info : kSettings) {
    if (key == info.key) {
      id = info.id;
      return true;
    }
  }
  return false;
}

bool isSwitch(SettingId id) { return id == SettingId::StayAwake; }

bool applySetting(Settings& s, SettingId id, std::string_view payload) {
  payload = trim(payload);
  if (isSwitch(id)) {
    bool on;
    if (equalsIgnoreCase(payload, "on") || equalsIgnoreCase(payload, "true") || payload == "1") {
      on = true;
    } else if (equalsIgnoreCase(payload, "off") || equalsIgnoreCase(payload, "false") || payload == "0") {
      on = false;
    } else {
      return false;
    }
    s.stayAwake = on;
    return true;
  }
  long v;
  if (!parseNumber(payload, v)) return false;
  const SettingInfo& info = settingInfo(id);
  if (v < info.min) v = info.min;
  if (v > info.max) v = info.max;
  switch (id) {
    case SettingId::Brightness: s.brightness = static_cast<uint8_t>(v); break;
    case SettingId::Speed: s.speed = static_cast<uint8_t>(v); break;
    case SettingId::Duration: s.duration = static_cast<uint16_t>(v); break;
    case SettingId::Linger: s.linger = static_cast<uint16_t>(v); break;
    case SettingId::WakeInterval: s.wakeInterval = static_cast<uint16_t>(v); break;
    case SettingId::StayAwake: break;
  }
  return true;
}

std::string settingValue(const Settings& s, SettingId id) {
  switch (id) {
    case SettingId::Brightness: return std::to_string(s.brightness);
    case SettingId::Speed: return std::to_string(s.speed);
    case SettingId::Duration: return std::to_string(s.duration);
    case SettingId::Linger: return std::to_string(s.linger);
    case SettingId::WakeInterval: return std::to_string(s.wakeInterval);
    case SettingId::StayAwake: return s.stayAwake ? "ON" : "OFF";
  }
  return "";
}

}  // namespace lb
