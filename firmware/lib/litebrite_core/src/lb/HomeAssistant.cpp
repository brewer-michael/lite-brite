#include "lb/HomeAssistant.h"

#include <ArduinoJson.h>

#include <cstdio>

#include "lb/Settings.h"

namespace lb {

namespace {

JsonObject component(JsonObject cmps, const Topics& topics, const char* key, const char* platform,
                     const char* name) {
  JsonObject c = cmps[key].to<JsonObject>();
  c["p"] = platform;
  c["name"] = name;
  c["unique_id"] = topics.uniqueId() + "_" + key;
  return c;
}

}  // namespace

std::string discoveryPayload(const Topics& topics, const DeviceInfo& info) {
  JsonDocument doc;

  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add(topics.uniqueId());
  dev["name"] = info.name;
  dev["manufacturer"] = "lite-brite (DIY)";
  dev["model"] = info.model;
  if (!info.version.empty()) dev["sw_version"] = info.version;
  if (!info.hardware.empty()) dev["hw_version"] = info.hardware;

  JsonObject origin = doc["o"].to<JsonObject>();
  origin["name"] = "lite-brite";
  if (!info.version.empty()) origin["sw_version"] = info.version;
  if (!info.supportUrl.empty()) origin["support_url"] = info.supportUrl;

  doc["qos"] = 1;
  JsonObject cmps = doc["cmps"].to<JsonObject>();

  // A text box for quick messages. It shares the "text" slot's retained topic,
  // so it empties itself once the sign has shown the message.
  JsonObject message = component(cmps, topics, "message", "text", "Message");
  message["command_topic"] = topics.message("text");
  message["state_topic"] = topics.message("text");
  message["retain"] = true;
  message["max"] = 255;
  message["icon"] = "mdi:message-text-outline";

  for (size_t i = 0; i < kSettingCount; ++i) {
    const SettingInfo& s = settingInfoAt(i);
    const bool sw = isSwitch(s.id);
    JsonObject c = component(cmps, topics, s.key, sw ? "switch" : "number", s.name);
    c["command_topic"] = topics.setting(s.key);
    c["state_topic"] = topics.setting(s.key);
    c["retain"] = true;
    c["entity_category"] = "config";
    c["icon"] = s.icon;
    if (sw) {
      c["payload_on"] = "ON";
      c["payload_off"] = "OFF";
    } else {
      c["min"] = s.min;
      c["max"] = s.max;
      c["step"] = 1;
      c["mode"] = s.id == SettingId::Brightness ? "slider" : "box";
      if (s.unit != nullptr) c["unit_of_measurement"] = s.unit;
    }
  }

  // Buttons publish retained commands so a sleeping sign still gets them.
  struct Button {
    const char* key;
    const char* name;
    const char* payload;
    const char* icon;
    bool diagnostic;
    bool enabled;
  };
  const Button buttons[] = {
      {"clear", "Clear messages", "clear", "mdi:message-off-outline", false, true},
      {"test", "Show test message", "test", "mdi:message-flash-outline", true, true},
      {"wiring", "Wiring test", "wiring", "mdi:led-strip-variant", true, false},
      {"reboot", "Restart", "reboot", "mdi:restart", true, false},
  };
  for (const Button& b : buttons) {
    JsonObject c = component(cmps, topics, b.key, "button", b.name);
    c["command_topic"] = topics.command();
    c["payload_press"] = b.payload;
    c["retain"] = true;
    c["icon"] = b.icon;
    if (b.diagnostic) c["entity_category"] = "diagnostic";
    if (!b.enabled) c["enabled_by_default"] = false;
  }

  const std::string state = topics.state();
  if (info.batterySensor) {
    JsonObject battery = component(cmps, topics, "battery", "sensor", "Battery");
    battery["state_topic"] = state;
    battery["value_template"] = "{{ value_json.battery }}";
    battery["device_class"] = "battery";
    battery["unit_of_measurement"] = "%";
    battery["state_class"] = "measurement";

    JsonObject voltage = component(cmps, topics, "voltage", "sensor", "Battery voltage");
    voltage["state_topic"] = state;
    voltage["value_template"] = "{{ value_json.voltage }}";
    voltage["device_class"] = "voltage";
    voltage["unit_of_measurement"] = "V";
    voltage["state_class"] = "measurement";
    voltage["suggested_display_precision"] = 2;
    voltage["entity_category"] = "diagnostic";
  }

  JsonObject rssi = component(cmps, topics, "rssi", "sensor", "Wi-Fi signal");
  rssi["state_topic"] = state;
  rssi["value_template"] = "{{ value_json.rssi }}";
  rssi["device_class"] = "signal_strength";
  rssi["unit_of_measurement"] = "dBm";
  rssi["state_class"] = "measurement";
  rssi["entity_category"] = "diagnostic";
  rssi["enabled_by_default"] = false;

  JsonObject pending = component(cmps, topics, "pending", "sensor", "Waiting messages");
  pending["state_topic"] = state;
  pending["value_template"] = "{{ value_json.pending }}";
  pending["state_class"] = "measurement";
  pending["icon"] = "mdi:message-badge-outline";

  JsonObject wake = component(cmps, topics, "wake", "sensor", "Last wake reason");
  wake["state_topic"] = state;
  wake["value_template"] = "{{ value_json.wake }}";
  wake["device_class"] = "enum";
  JsonArray options = wake["options"].to<JsonArray>();
  for (const char* o : {"power_on", "motion", "timer", "other"}) options.add(o);
  wake["entity_category"] = "diagnostic";
  wake["icon"] = "mdi:alarm";

  JsonObject awake = component(cmps, topics, "awake", "binary_sensor", "Awake");
  awake["state_topic"] = topics.awake();
  awake["payload_on"] = "ON";
  awake["payload_off"] = "OFF";
  awake["device_class"] = "connectivity";
  awake["entity_category"] = "diagnostic";

  if (info.motionSensor) {
    JsonObject motion = component(cmps, topics, "motion", "binary_sensor", "Motion");
    motion["state_topic"] = topics.motion();
    motion["payload_on"] = "ON";
    motion["payload_off"] = "OFF";
    motion["device_class"] = "motion";
    motion["off_delay"] = 30;
  }

  if (info.usbSense) {
    JsonObject usb = component(cmps, topics, "usb", "binary_sensor", "USB power");
    usb["state_topic"] = state;
    usb["value_template"] = "{{ 'ON' if value_json.usb else 'OFF' }}";
    usb["device_class"] = "plug";
    usb["entity_category"] = "diagnostic";
  }

  JsonObject event = component(cmps, topics, "shown", "event", "Message");
  event["state_topic"] = topics.event();
  JsonArray types = event["event_types"].to<JsonArray>();
  types.add("shown");
  types.add("expired");
  types.add("invalid");
  event["icon"] = "mdi:message-check-outline";

  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string statePayload(const DeviceStatus& status) {
  JsonDocument doc;
  if (status.batteryPercent >= 0) {
    doc["battery"] = status.batteryPercent;
    char volts[8];
    std::snprintf(volts, sizeof(volts), "%.2f", status.millivolts / 1000.0);
    doc["voltage"] = serialized(std::string(volts));
  } else {
    doc["battery"] = nullptr;
    doc["voltage"] = nullptr;
  }
  doc["battery_level"] = status.batteryLevel;
  doc["rssi"] = status.rssi;
  doc["wake"] = status.wake;
  doc["pending"] = status.pending;
  doc["usb"] = status.usb;
  doc["wakes"] = status.wakes;
  doc["version"] = status.version;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string eventPayload(const char* eventType, const Message& msg) {
  JsonDocument doc;
  doc["event_type"] = eventType;
  doc["slot"] = msg.slot;
  doc["text"] = msg.text;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string invalidEventPayload(const std::string& slot, const std::string& error) {
  JsonDocument doc;
  doc["event_type"] = "invalid";
  doc["slot"] = slot;
  doc["error"] = error;
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace lb
