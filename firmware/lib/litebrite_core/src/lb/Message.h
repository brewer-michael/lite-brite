#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "lb/Color.h"

namespace lb {

enum class Effect : uint8_t {
  Scroll,  // ticker scroll (short text that fits is shown centered)
  Flash,   // attention-grabbing inverse flashes, then scroll
  Blink,   // text blinks on and off
  Pulse,   // text breathes brighter and dimmer
  Static,  // no motion if it fits, otherwise scrolls
};

enum class ShowWhen : uint8_t {
  Motion,  // wait until someone is in front of the sign (default)
  Now,     // show as soon as the sign is awake
};

// One message the sign can show. Home Assistant publishes these as retained
// MQTT messages on lite-brite/<device>/msg/<slot>; see docs/mqtt-api.md.
struct Message {
  std::string slot;               // the topic suffix; one message per slot
  std::string text;               // UTF-8 with optional :icon: / {color} markup
  Rgb color = colors::kWhite;
  bool rainbow = false;           // color: "rainbow"
  Rgb background = colors::kBlack;
  Effect effect = Effect::Scroll;
  uint16_t speed = 0;             // scroll speed in px/s, 0 = device setting
  uint16_t durationS = 0;         // how long to show it, 0 = device setting
  uint8_t repeat = 0;             // number of passes; overrides duration when > 0
  int8_t priority = 0;            // higher shows first
  bool once = true;               // clear it from the broker after it's been shown
  uint8_t brightness = 0;         // 1-100 %, 0 = device setting
  int64_t expires = 0;            // unix time after which it's dropped, 0 = never
  ShowWhen when = ShowWhen::Motion;
  uint32_t seq = 0;               // arrival order, assigned by MessageQueue
};

constexpr size_t kMaxTextBytes = 400;
constexpr uint16_t kMaxDurationS = 600;

const char* effectName(Effect effect);
bool parseEffect(std::string_view text, Effect& out);

// Parses a payload into `out`. A JSON object uses the documented fields; any
// other payload is taken as the message text with default styling. Returns
// false (with a human-readable reason in `error`) if the payload can't become
// a message. Unknown fields are ignored; out-of-range numbers are clamped.
bool parseMessage(std::string_view slot, std::string_view payload, Message& out, std::string& error);

// Parses an ISO-8601 timestamp such as "2026-09-27T17:05:00-07:00" or
// "2026-09-27T17:05:00.123456+00:00" or "...Z" into unix seconds. A timestamp
// without an offset is taken as UTC.
bool parseIsoTime(std::string_view text, int64_t& unixSeconds);

}  // namespace lb
