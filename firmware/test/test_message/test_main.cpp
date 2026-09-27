#include <unity.h>

#include <string>

#include "lb/Message.h"
#include "lb/MessageQueue.h"

using namespace lb;

void setUp() {}
void tearDown() {}

static Message parse(const std::string& payload) {
  Message m;
  std::string error;
  TEST_ASSERT_TRUE_MESSAGE(parseMessage("slot", payload, m, error), error.c_str());
  return m;
}

static std::string parseError(const std::string& payload) {
  Message m;
  std::string error;
  TEST_ASSERT_FALSE(parseMessage("slot", payload, m, error));
  return error;
}

void test_plain_text_payload() {
  const Message m = parse("  Keys away first!  ");
  TEST_ASSERT_EQUAL_STRING("slot", m.slot.c_str());
  TEST_ASSERT_EQUAL_STRING("Keys away first!", m.text.c_str());
  TEST_ASSERT_TRUE(m.color == colors::kWhite);
  TEST_ASSERT_EQUAL(Effect::Scroll, m.effect);
  TEST_ASSERT_TRUE(m.once);
  TEST_ASSERT_EQUAL(ShowWhen::Motion, m.when);
}

void test_json_string_payload() {
  const Message m = parse("\"Hello there\"");
  TEST_ASSERT_EQUAL_STRING("Hello there", m.text.c_str());
}

void test_full_json_payload() {
  const Message m = parse(R"({
    "text": "Welcome home! :key: Keys away first",
    "color": "orange", "background": [0, 0, 40], "effect": "flash",
    "speed": 30, "duration": 45, "repeat": 2, "priority": 5, "once": false,
    "brightness": 80, "expires": 1790553900, "when": "now", "extra": "ignored"
  })");
  TEST_ASSERT_EQUAL_STRING("Welcome home! :key: Keys away first", m.text.c_str());
  TEST_ASSERT_TRUE(m.color == Rgb(255, 165, 0));
  TEST_ASSERT_TRUE(m.background == Rgb(0, 0, 40));
  TEST_ASSERT_EQUAL(Effect::Flash, m.effect);
  TEST_ASSERT_EQUAL_UINT16(30, m.speed);
  TEST_ASSERT_EQUAL_UINT16(45, m.durationS);
  TEST_ASSERT_EQUAL_UINT8(2, m.repeat);
  TEST_ASSERT_EQUAL_INT8(5, m.priority);
  TEST_ASSERT_FALSE(m.once);
  TEST_ASSERT_EQUAL_UINT8(80, m.brightness);
  TEST_ASSERT_EQUAL_INT64(1790553900, m.expires);
  TEST_ASSERT_EQUAL(ShowWhen::Now, m.when);
}

void test_message_alias_and_color_forms() {
  Message m = parse(R"({"message": "Hi", "color": [10, 20, 300]})");
  TEST_ASSERT_EQUAL_STRING("Hi", m.text.c_str());
  TEST_ASSERT_TRUE(m.color == Rgb(10, 20, 255));
  m = parse(R"({"text": "Hi", "color": {"r": 1, "g": 2, "b": 3}})");
  TEST_ASSERT_TRUE(m.color == Rgb(1, 2, 3));
  m = parse(R"({"text": "Hi", "color": "#00FF00"})");
  TEST_ASSERT_TRUE(m.color == Rgb(0, 255, 0));
  m = parse(R"({"text": "Hi", "color": "rainbow"})");
  TEST_ASSERT_TRUE(m.rainbow);
  m = parse(R"({"text": "Hi", "color": "not-a-color"})");
  TEST_ASSERT_TRUE(m.color == colors::kWhite);
}

void test_numbers_as_strings_and_clamping() {
  const Message m = parse(R"({"text": "Hi", "speed": "500", "brightness": 0, "duration": "2000",
                              "repeat": -3, "priority": 1000})");
  TEST_ASSERT_EQUAL_UINT16(120, m.speed);
  TEST_ASSERT_EQUAL_UINT8(0, m.brightness);  // 0 means "use the device setting"
  TEST_ASSERT_EQUAL_UINT16(kMaxDurationS, m.durationS);
  TEST_ASSERT_EQUAL_UINT8(0, m.repeat);
  TEST_ASSERT_EQUAL_INT8(100, m.priority);
}

void test_numeric_text_is_shown() {
  const Message m = parse(R"({"text": 42})");
  TEST_ASSERT_EQUAL_STRING("42", m.text.c_str());
}

void test_icon_field_prepends_icon() {
  const Message m = parse(R"({"text": "Keys away", "icon": "Key"})");
  TEST_ASSERT_EQUAL_STRING(":key: Keys away", m.text.c_str());
  const Message unknown = parse(R"({"text": "Keys away", "icon": "unicorn"})");
  TEST_ASSERT_EQUAL_STRING("Keys away", unknown.text.c_str());
}

void test_booleans_in_many_forms() {
  TEST_ASSERT_FALSE(parse(R"({"text": "a", "once": "no"})").once);
  TEST_ASSERT_FALSE(parse(R"({"text": "a", "once": 0})").once);
  TEST_ASSERT_TRUE(parse(R"({"text": "a", "once": "ON"})").once);
}

void test_expiry_formats() {
  TEST_ASSERT_EQUAL_INT64(1790553900, parse(R"({"text": "a", "expires": "2026-09-27T17:05:00-07:00"})").expires);
  TEST_ASSERT_EQUAL_INT64(1790553900,
                          parse(R"({"text": "a", "expires": "2026-09-28T00:05:00.123456+00:00"})").expires);
  TEST_ASSERT_EQUAL_INT64(1790553900, parse(R"({"text": "a", "expires": "2026-09-28T00:05:00Z"})").expires);
  TEST_ASSERT_EQUAL_INT64(1790553900, parse(R"({"text": "a", "expires": 1790553900000})").expires);
  TEST_ASSERT_EQUAL_INT64(0, parse(R"({"text": "a", "expires": 0})").expires);
  TEST_ASSERT_EQUAL_INT64(32503680000LL, parse(R"({"text": "a", "expires": 1e308})").expires);
  TEST_ASSERT_EQUAL_INT64(1790553900, parse(R"({"text": "a", "sent": 1790553900})").sent);
  TEST_ASSERT_EQUAL_INT64(0, parse(R"({"text": "a", "sent": "garbage"})").sent);  // ignored, not fatal
}

void test_iso_time_parser() {
  int64_t t = 0;
  TEST_ASSERT_TRUE(parseIsoTime("2000-02-29T12:00:00", t));
  TEST_ASSERT_EQUAL_INT64(951825600, t);
  TEST_ASSERT_TRUE(parseIsoTime("1970-01-01 00:00", t));
  TEST_ASSERT_EQUAL_INT64(0, t);
  TEST_ASSERT_FALSE(parseIsoTime("2026-13-01T00:00:00", t));
  TEST_ASSERT_FALSE(parseIsoTime("tomorrow", t));
  TEST_ASSERT_FALSE(parseIsoTime("2026-09-28T00:05:00+0", t));
}

void test_effects() {
  Effect e;
  TEST_ASSERT_TRUE(parseEffect("Flash", e));
  TEST_ASSERT_EQUAL(Effect::Flash, e);
  TEST_ASSERT_TRUE(parseEffect("breathe", e));
  TEST_ASSERT_EQUAL(Effect::Pulse, e);
  TEST_ASSERT_FALSE(parseEffect("explode", e));
  TEST_ASSERT_TRUE(parseEffect("static", e));  // alias: text that fits stays still anyway
  TEST_ASSERT_EQUAL(Effect::Scroll, e);
  TEST_ASSERT_TRUE(parseEffect("Ticker", e));
  TEST_ASSERT_EQUAL(Effect::Ticker, e);
  TEST_ASSERT_EQUAL_STRING("ticker", effectName(Effect::Ticker));
  TEST_ASSERT_EQUAL(Effect::Scroll, parse(R"({"text": "a", "effect": "explode"})").effect);
}

void test_rejected_payloads() {
  TEST_ASSERT_EQUAL_STRING("empty payload", parseError("   ").c_str());
  TEST_ASSERT_EQUAL_STRING("missing \"text\"", parseError(R"({"color": "red"})").c_str());
  TEST_ASSERT_EQUAL_STRING("empty text", parseError(R"({"text": "   "})").c_str());
  TEST_ASSERT_TRUE(parseError("{\"text\": ").rfind("invalid JSON", 0) == 0);
  TEST_ASSERT_TRUE(parseError(R"({"text": "a", "expires": "someday"})").rfind("can't read", 0) == 0);
}

void test_long_text_is_truncated_on_a_character_boundary() {
  std::string text(kMaxTextBytes - 1, 'a');
  text += "\xC3\xA9\xC3\xA9";  // two 2-byte characters straddling the limit
  const Message m = parse(text);
  TEST_ASSERT_EQUAL(kMaxTextBytes - 1, m.text.size());
}

// ---- MessageQueue ----------------------------------------------------------

static Message make(const char* slot, int8_t priority = 0, int64_t expires = 0) {
  Message m;
  m.slot = slot;
  m.text = slot;
  m.priority = priority;
  m.expires = expires;
  return m;
}

void test_queue_upsert_replace_remove() {
  MessageQueue q;
  TEST_ASSERT_TRUE(q.upsert(make("a")));
  TEST_ASSERT_TRUE(q.upsert(make("b")));
  Message changed = make("a");
  changed.text = "new";
  TEST_ASSERT_TRUE(q.upsert(changed));
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_STRING("new", q.find("a")->text.c_str());
  TEST_ASSERT_TRUE(q.find("a")->seq > q.find("b")->seq);
  TEST_ASSERT_TRUE(q.remove("a"));
  TEST_ASSERT_FALSE(q.remove("a"));
  TEST_ASSERT_NULL(q.find("a"));
  TEST_ASSERT_EQUAL(1, q.size());
}

void test_queue_orders_by_priority_then_arrival() {
  MessageQueue q;
  q.upsert(make("low", -1));
  q.upsert(make("first", 0));
  q.upsert(make("urgent", 9));
  q.upsert(make("second", 0));
  const auto order = q.ordered(0);
  TEST_ASSERT_EQUAL(4, order.size());
  TEST_ASSERT_EQUAL_STRING("urgent", order[0]->slot.c_str());
  TEST_ASSERT_EQUAL_STRING("first", order[1]->slot.c_str());
  TEST_ASSERT_EQUAL_STRING("second", order[2]->slot.c_str());
  TEST_ASSERT_EQUAL_STRING("low", order[3]->slot.c_str());
}

void test_queue_expiry_needs_a_clock() {
  MessageQueue q;
  q.upsert(make("stale", 0, 1000));
  q.upsert(make("fresh", 0, 5000));
  q.upsert(make("forever"));
  TEST_ASSERT_EQUAL(3, q.ordered(0).size());  // clock unknown: show everything
  TEST_ASSERT_EQUAL(0, q.expired(0).size());
  TEST_ASSERT_EQUAL(2, q.ordered(2000).size());
  const auto gone = q.expired(2000);
  TEST_ASSERT_EQUAL(1, gone.size());
  TEST_ASSERT_EQUAL_STRING("stale", gone[0].c_str());
}

void test_queue_capacity_evicts_lowest_priority() {
  MessageQueue q;
  for (size_t i = 0; i < MessageQueue::kCapacity; ++i) {
    TEST_ASSERT_TRUE(q.upsert(make(("m" + std::to_string(i)).c_str(), i == 3 ? -5 : 0)));
  }
  TEST_ASSERT_FALSE(q.upsert(make("too-low", -9)));
  TEST_ASSERT_TRUE(q.upsert(make("important", 1)));
  TEST_ASSERT_EQUAL(MessageQueue::kCapacity, q.size());
  TEST_ASSERT_NULL(q.find("m3"));
  TEST_ASSERT_NOT_NULL(q.find("important"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_plain_text_payload);
  RUN_TEST(test_json_string_payload);
  RUN_TEST(test_full_json_payload);
  RUN_TEST(test_message_alias_and_color_forms);
  RUN_TEST(test_numbers_as_strings_and_clamping);
  RUN_TEST(test_numeric_text_is_shown);
  RUN_TEST(test_icon_field_prepends_icon);
  RUN_TEST(test_booleans_in_many_forms);
  RUN_TEST(test_expiry_formats);
  RUN_TEST(test_iso_time_parser);
  RUN_TEST(test_effects);
  RUN_TEST(test_rejected_payloads);
  RUN_TEST(test_long_text_is_truncated_on_a_character_boundary);
  RUN_TEST(test_queue_upsert_replace_remove);
  RUN_TEST(test_queue_orders_by_priority_then_arrival);
  RUN_TEST(test_queue_expiry_needs_a_clock);
  RUN_TEST(test_queue_capacity_evicts_lowest_priority);
  return UNITY_END();
}
