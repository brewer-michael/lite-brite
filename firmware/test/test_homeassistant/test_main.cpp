#include <ArduinoJson.h>
#include <unity.h>

#include <string>

#include "lb/HomeAssistant.h"
#include "lb/Settings.h"
#include "lb/Topics.h"

using namespace lb;

void setUp() {}
void tearDown() {}

void test_topic_layout() {
  const Topics t("lite-brite", "entryway");
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway", t.base().c_str());
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/msg/keys", t.message("keys").c_str());
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/msg/+", t.messageFilter().c_str());
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/config/brightness", t.setting("brightness").c_str());
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/cmd", t.command().c_str());
  TEST_ASSERT_EQUAL_STRING("homeassistant/device/lite_brite_entryway/config",
                           t.discovery("homeassistant").c_str());
  TEST_ASSERT_EQUAL_STRING("lite_brite_front_hall", Topics("x", "Front-Hall").uniqueId().c_str());
}

void test_topic_classification() {
  const Topics t("lite-brite", "entryway");
  std::string_view rest;
  TEST_ASSERT_EQUAL(Topics::Kind::Message, t.classify("lite-brite/entryway/msg/keys", rest));
  TEST_ASSERT_EQUAL_STRING("keys", std::string(rest).c_str());
  TEST_ASSERT_EQUAL(Topics::Kind::Setting, t.classify("lite-brite/entryway/config/speed", rest));
  TEST_ASSERT_EQUAL_STRING("speed", std::string(rest).c_str());
  TEST_ASSERT_EQUAL(Topics::Kind::Command, t.classify("lite-brite/entryway/cmd", rest));
  TEST_ASSERT_EQUAL(Topics::Kind::Sync, t.classify("lite-brite/entryway/sync", rest));
  TEST_ASSERT_EQUAL(Topics::Kind::Unknown, t.classify("lite-brite/entryway/msg/bad slot", rest));
  TEST_ASSERT_EQUAL(Topics::Kind::Unknown, t.classify("lite-brite/entryway2/msg/keys", rest));
  TEST_ASSERT_EQUAL(Topics::Kind::Unknown, t.classify("lite-brite/entryway", rest));
  TEST_ASSERT_EQUAL(Topics::Kind::Unknown, t.classify("lite-brite/entryway/state", rest));
}

void test_names() {
  TEST_ASSERT_TRUE(isValidName("keys"));
  TEST_ASSERT_TRUE(isValidName("Trash-Night_2"));
  TEST_ASSERT_FALSE(isValidName(""));
  TEST_ASSERT_FALSE(isValidName("a/b"));
  TEST_ASSERT_FALSE(isValidName("with space"));
  TEST_ASSERT_FALSE(isValidName(std::string(33, 'a')));
}

static JsonDocument discovery(const DeviceInfo& info) {
  const Topics t("lite-brite", "entryway");
  const std::string payload = discoveryPayload(t, info);
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, payload));
  return doc;
}

void test_discovery_device_and_origin() {
  DeviceInfo info;
  info.name = "Entryway sign";
  info.version = "1.2.3";
  info.hardware = "XIAO-ESP32S3";
  JsonDocument doc = discovery(info);
  TEST_ASSERT_EQUAL_STRING("lite_brite_entryway", doc["dev"]["identifiers"][0]);
  TEST_ASSERT_EQUAL_STRING("Entryway sign", doc["dev"]["name"]);
  TEST_ASSERT_EQUAL_STRING("1.2.3", doc["dev"]["sw_version"]);
  TEST_ASSERT_EQUAL_STRING("lite-brite", doc["o"]["name"]);
  TEST_ASSERT_EQUAL_INT(1, doc["qos"].as<int>());
}

void test_discovery_components() {
  JsonDocument doc = discovery(DeviceInfo{});
  JsonObject cmps = doc["cmps"];
  // Every component has a platform, a name and a unique id.
  for (JsonPair kv : cmps) {
    TEST_ASSERT_TRUE_MESSAGE(kv.value()["p"].is<const char*>(), kv.key().c_str());
    TEST_ASSERT_TRUE_MESSAGE(kv.value()["name"].is<const char*>(), kv.key().c_str());
    const std::string uid = kv.value()["unique_id"].as<std::string>();
    TEST_ASSERT_EQUAL_STRING((std::string("lite_brite_entryway_") + kv.key().c_str()).c_str(), uid.c_str());
  }
  TEST_ASSERT_EQUAL_STRING("text", cmps["message"]["p"]);
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/msg/text", cmps["message"]["command_topic"]);
  TEST_ASSERT_TRUE(cmps["message"]["retain"].as<bool>());

  TEST_ASSERT_EQUAL_STRING("number", cmps["brightness"]["p"]);
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/config/brightness", cmps["brightness"]["command_topic"]);
  TEST_ASSERT_EQUAL_STRING("lite-brite/entryway/config/brightness", cmps["brightness"]["state_topic"]);
  TEST_ASSERT_EQUAL_INT(100, cmps["brightness"]["max"].as<int>());
  TEST_ASSERT_EQUAL_STRING("switch", cmps["stay_awake"]["p"]);
  for (size_t i = 0; i < kSettingCount; ++i) {
    TEST_ASSERT_FALSE(cmps[settingInfoAt(i).key].isNull());
  }

  TEST_ASSERT_EQUAL_STRING("button", cmps["clear"]["p"]);
  TEST_ASSERT_EQUAL_STRING("clear", cmps["clear"]["payload_press"]);
  TEST_ASSERT_TRUE(cmps["clear"]["retain"].as<bool>());

  TEST_ASSERT_EQUAL_STRING("battery", cmps["battery"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("{{ value_json.battery }}", cmps["battery"]["value_template"]);
  TEST_ASSERT_EQUAL_STRING("motion", cmps["motion"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("event", cmps["shown"]["p"]);
  TEST_ASSERT_TRUE(cmps["usb"].isNull());
}

void test_discovery_respects_hardware_options() {
  DeviceInfo info;
  info.motionSensor = false;
  info.batterySensor = false;
  info.usbSense = true;
  JsonDocument doc = discovery(info);
  TEST_ASSERT_TRUE(doc["cmps"]["motion"].isNull());
  TEST_ASSERT_TRUE(doc["cmps"]["battery"].isNull());
  TEST_ASSERT_TRUE(doc["cmps"]["voltage"].isNull());
  TEST_ASSERT_EQUAL_STRING("plug", doc["cmps"]["usb"]["device_class"]);
}

void test_state_payload() {
  DeviceStatus s;
  s.batteryPercent = 87;
  s.millivolts = 4023;
  s.batteryLevel = "ok";
  s.rssi = -61;
  s.wake = "motion";
  s.pending = 2;
  s.wakes = 42;
  s.version = "0.1.0";
  const std::string payload = statePayload(s);
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, payload));
  TEST_ASSERT_EQUAL_INT(87, doc["battery"].as<int>());
  TEST_ASSERT_TRUE(payload.find("\"voltage\":4.02") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("motion", doc["wake"]);
  TEST_ASSERT_EQUAL_INT(2, doc["pending"].as<int>());
  TEST_ASSERT_FALSE(doc["usb"].as<bool>());

  DeviceStatus unknown;
  JsonDocument doc2;
  TEST_ASSERT_FALSE(deserializeJson(doc2, statePayload(unknown)));
  TEST_ASSERT_TRUE(doc2["battery"].isNull());
}

void test_event_payload() {
  Message m;
  m.slot = "keys";
  m.text = "Keys \"away\"";
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, eventPayload("shown", m)));
  TEST_ASSERT_EQUAL_STRING("shown", doc["event_type"]);
  TEST_ASSERT_EQUAL_STRING("keys", doc["slot"]);
  TEST_ASSERT_EQUAL_STRING("Keys \"away\"", doc["text"]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_topic_layout);
  RUN_TEST(test_topic_classification);
  RUN_TEST(test_names);
  RUN_TEST(test_discovery_device_and_origin);
  RUN_TEST(test_discovery_components);
  RUN_TEST(test_discovery_respects_hardware_options);
  RUN_TEST(test_state_payload);
  RUN_TEST(test_event_payload);
  return UNITY_END();
}
