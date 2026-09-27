#pragma once

#include <string>
#include <string_view>

namespace lb {

// MQTT topic layout (see docs/mqtt-api.md). With prefix "lite-brite" and
// device "entryway":
//
//   lite-brite/entryway/msg/<slot>     retained message per slot (HA -> sign)
//   lite-brite/entryway/config/<key>   retained settings (HA <-> sign)
//   lite-brite/entryway/cmd            retained one-shot command (HA -> sign)
//   lite-brite/entryway/state          retained JSON status (sign -> HA)
//   lite-brite/entryway/awake          retained ON/OFF, also the MQTT will
//   lite-brite/entryway/motion         ON when motion wakes the sign
//   lite-brite/entryway/event          {"event_type": "shown", ...}
//   lite-brite/entryway/sync           the sign's own round-trip marker
class Topics {
 public:
  Topics(std::string_view prefix, std::string_view deviceId);

  const std::string& base() const { return base_; }
  const std::string& deviceId() const { return deviceId_; }

  std::string message(std::string_view slot) const { return base_ + "/msg/" + std::string(slot); }
  std::string messageFilter() const { return base_ + "/msg/+"; }
  std::string setting(std::string_view key) const { return base_ + "/config/" + std::string(key); }
  std::string settingFilter() const { return base_ + "/config/+"; }
  std::string command() const { return base_ + "/cmd"; }
  std::string state() const { return base_ + "/state"; }
  std::string awake() const { return base_ + "/awake"; }
  std::string motion() const { return base_ + "/motion"; }
  std::string event() const { return base_ + "/event"; }
  std::string sync() const { return base_ + "/sync"; }

  // Home Assistant device-discovery topic: <prefix>/device/<unique id>/config
  std::string discovery(std::string_view discoveryPrefix) const;

  // "lite_brite_entryway": stable id for Home Assistant's device registry.
  const std::string& uniqueId() const { return uniqueId_; }

  enum class Kind { Message, Setting, Command, Sync, Unknown };
  // Classifies an incoming topic; `rest` receives the slot or setting key.
  Kind classify(std::string_view topic, std::string_view& rest) const;

 private:
  std::string deviceId_;
  std::string base_;
  std::string uniqueId_;
};

// Slots and device ids: 1-32 characters of A-Z a-z 0-9 _ -
bool isValidName(std::string_view name);

}  // namespace lb
