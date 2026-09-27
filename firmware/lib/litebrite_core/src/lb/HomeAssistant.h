#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "lb/Message.h"
#include "lb/Topics.h"

namespace lb {

struct DeviceInfo {
  std::string name = "Lite-Brite";  // Home Assistant device name
  std::string model = "LED sign";
  std::string hardware;             // board name
  std::string version;              // firmware version
  std::string supportUrl;
  bool motionSensor = true;
  bool batterySensor = true;
  bool usbSense = false;
};

// Home Assistant MQTT device-discovery payload: one retained message that
// creates the sign's device and all of its entities. Publish it to
// topics.discovery("homeassistant").
std::string discoveryPayload(const Topics& topics, const DeviceInfo& info);

struct DeviceStatus {
  int batteryPercent = -1;  // -1 = unknown
  uint16_t millivolts = 0;
  const char* batteryLevel = "unknown";
  int rssi = 0;
  const char* wake = "power_on";
  size_t pending = 0;
  bool usb = false;
  uint32_t wakes = 0;
  bool clockSet = false;  // message expiry needs the clock
  const char* version = "";
};

// Retained JSON on topics.state(); the sensors read fields from it.
std::string statePayload(const DeviceStatus& status);

// {"event_type": "shown", "slot": "keys", "text": "..."} for the event entity.
std::string eventPayload(const char* eventType, const Message& msg);

// {"event_type": "invalid", "slot": "keys", "error": "..."}: a payload the
// sign couldn't use, so automation mistakes are visible in Home Assistant.
std::string invalidEventPayload(const std::string& slot, const std::string& error);

}  // namespace lb
