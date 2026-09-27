#include "lb/Message.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "lb/Icons.h"

namespace lb {

namespace {

std::string_view trim(std::string_view s) {
  auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!s.empty() && space(s.front())) s.remove_prefix(1);
  while (!s.empty() && space(s.back())) s.remove_suffix(1);
  return s;
}

bool equalsIgnoreCase(std::string_view a, const char* b) {
  const size_t n = std::strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; ++i) {
    char c = a[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != b[i]) return false;
  }
  return true;
}

// Cuts `s` to at most `max` bytes without splitting a UTF-8 sequence.
void truncateUtf8(std::string& s, size_t max) {
  if (s.size() <= max) return;
  size_t cut = max;
  while (cut > 0 && (static_cast<uint8_t>(s[cut]) & 0xC0) == 0x80) --cut;
  s.resize(cut);
}

bool readNumber(JsonVariantConst v, double& out) {
  if (v.is<double>()) {
    out = v.as<double>();
    return std::isfinite(out);
  }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    char* end = nullptr;
    const double d = std::strtod(s, &end);
    if (end == s) return false;
    while (*end == ' ') ++end;
    if (*end != '\0' || !std::isfinite(d)) return false;
    out = d;
    return true;
  }
  return false;
}

template <typename T>
bool readClamped(JsonVariantConst v, double lo, double hi, T& out) {
  double d;
  if (!readNumber(v, d)) return false;
  if (d < lo) d = lo;
  if (d > hi) d = hi;
  out = static_cast<T>(std::lround(d));
  return true;
}

bool readBool(JsonVariantConst v, bool& out) {
  if (v.is<bool>()) {
    out = v.as<bool>();
    return true;
  }
  double d;
  if (v.is<double>() && readNumber(v, d)) {
    out = d != 0;
    return true;
  }
  if (v.is<const char*>()) {
    const std::string_view s = trim(v.as<const char*>());
    for (const char* t : {"true", "yes", "on", "1"}) {
      if (equalsIgnoreCase(s, t)) {
        out = true;
        return true;
      }
    }
    for (const char* f : {"false", "no", "off", "0"}) {
      if (equalsIgnoreCase(s, f)) {
        out = false;
        return true;
      }
    }
  }
  return false;
}

// Accepts "orange", "#ff8800", "rainbow", [255, 136, 0] or {"r":..,"g":..,"b":..}.
bool readColor(JsonVariantConst v, Rgb& color, bool* rainbow) {
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (rainbow != nullptr && isRainbow(s)) {
      *rainbow = true;
      return true;
    }
    return parseColor(s, color);
  }
  JsonVariantConst r, g, b;
  if (v.is<JsonArrayConst>()) {
    JsonArrayConst a = v.as<JsonArrayConst>();
    if (a.size() != 3) return false;
    r = a[0];
    g = a[1];
    b = a[2];
  } else if (v.is<JsonObjectConst>()) {
    r = v["r"];
    g = v["g"];
    b = v["b"];
  } else {
    return false;
  }
  uint8_t rv, gv, bv;
  if (!readClamped(r, 0, 255, rv) || !readClamped(g, 0, 255, gv) || !readClamped(b, 0, 255, bv)) {
    return false;
  }
  color = Rgb(rv, gv, bv);
  return true;
}

bool readText(JsonVariantConst v, std::string& out) {
  if (v.is<const char*>()) {
    out = v.as<const char*>();
    return true;
  }
  if (v.is<double>() || v.is<bool>()) {  // {"text": 42} shows "42"
    out.clear();
    serializeJson(v, out);
    return true;
  }
  return false;
}

bool readExpiry(JsonVariantConst v, int64_t& out) {
  double d;
  if (readNumber(v, d)) {
    if (d <= 0) {
      out = 0;
      return true;
    }
    if (d > 1e11) d /= 1000.0;  // milliseconds (JavaScript-style timestamps)
    out = static_cast<int64_t>(d);
    return true;
  }
  if (v.is<const char*>()) return parseIsoTime(trim(v.as<const char*>()), out);
  return false;
}

bool parseWhen(std::string_view s, ShowWhen& out) {
  s = trim(s);
  for (const char* m : {"motion", "presence", "person"}) {
    if (equalsIgnoreCase(s, m)) {
      out = ShowWhen::Motion;
      return true;
    }
  }
  for (const char* n : {"now", "immediately", "always"}) {
    if (equalsIgnoreCase(s, n)) {
      out = ShowWhen::Now;
      return true;
    }
  }
  return false;
}

int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2 ? 1 : 0;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// Reads exactly `n` digits at s[pos].
bool digits(std::string_view s, size_t& pos, size_t n, int& out) {
  if (pos + n > s.size()) return false;
  int v = 0;
  for (size_t k = 0; k < n; ++k) {
    const char c = s[pos + k];
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  pos += n;
  out = v;
  return true;
}

bool expect(std::string_view s, size_t& pos, char c) {
  if (pos >= s.size() || s[pos] != c) return false;
  ++pos;
  return true;
}

}  // namespace

const char* effectName(Effect effect) {
  switch (effect) {
    case Effect::Scroll: return "scroll";
    case Effect::Flash: return "flash";
    case Effect::Blink: return "blink";
    case Effect::Pulse: return "pulse";
    case Effect::Static: return "static";
  }
  return "scroll";
}

bool parseEffect(std::string_view text, Effect& out) {
  text = trim(text);
  struct Name {
    const char* name;
    Effect effect;
  };
  static const Name kNames[] = {
      {"scroll", Effect::Scroll}, {"ticker", Effect::Scroll}, {"flash", Effect::Flash},
      {"blink", Effect::Blink},   {"pulse", Effect::Pulse},   {"breathe", Effect::Pulse},
      {"static", Effect::Static}, {"none", Effect::Static},   {"still", Effect::Static},
  };
  for (const auto& n : kNames) {
    if (equalsIgnoreCase(text, n.name)) {
      out = n.effect;
      return true;
    }
  }
  return false;
}

bool parseIsoTime(std::string_view s, int64_t& unixSeconds) {
  size_t p = 0;
  int year, month, day, hour, minute, second = 0;
  if (!digits(s, p, 4, year) || !expect(s, p, '-') || !digits(s, p, 2, month) || !expect(s, p, '-') ||
      !digits(s, p, 2, day)) {
    return false;
  }
  if (p >= s.size() || (s[p] != 'T' && s[p] != 't' && s[p] != ' ')) return false;
  ++p;
  if (!digits(s, p, 2, hour) || !expect(s, p, ':') || !digits(s, p, 2, minute)) return false;
  if (p < s.size() && s[p] == ':') {
    ++p;
    if (!digits(s, p, 2, second)) return false;
    if (p < s.size() && (s[p] == '.' || s[p] == ',')) {  // fractional seconds: ignore
      ++p;
      while (p < s.size() && s[p] >= '0' && s[p] <= '9') ++p;
    }
  }
  int offset = 0;
  if (p < s.size()) {
    const char sign = s[p];
    if (sign == 'Z' || sign == 'z') {
      ++p;
    } else if (sign == '+' || sign == '-') {
      ++p;
      int oh, om = 0;
      if (!digits(s, p, 2, oh)) return false;
      if (p < s.size() && s[p] == ':') ++p;
      if (p < s.size() && !digits(s, p, 2, om)) return false;
      offset = (oh * 3600 + om * 60) * (sign == '-' ? -1 : 1);
    } else {
      return false;
    }
  }
  if (p != s.size()) return false;
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
    return false;
  }
  const int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  unixSeconds = days * 86400 + hour * 3600 + minute * 60 + second - offset;
  return true;
}

bool parseMessage(std::string_view slot, std::string_view payload, Message& out, std::string& error) {
  out = Message();
  out.slot = std::string(slot);
  payload = trim(payload);
  if (payload.empty()) {
    error = "empty payload";
    return false;
  }

  if (payload.front() != '{' && payload.front() != '"') {
    out.text = std::string(payload);
  } else {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload.data(), payload.size());
    if (err) {
      error = std::string("invalid JSON: ") + err.c_str();
      return false;
    }
    if (doc.is<const char*>()) {  // a bare JSON string: "\"Hello\""
      out.text = doc.as<const char*>();
    } else if (!doc.is<JsonObjectConst>()) {
      error = "expected a JSON object or plain text";
      return false;
    } else {
      JsonObjectConst o = doc.as<JsonObjectConst>();
      JsonVariantConst text = o["text"];
      if (text.isNull()) text = o["message"];
      if (!readText(text, out.text)) {
        error = "missing \"text\"";
        return false;
      }
      if (!o["color"].isNull()) readColor(o["color"], out.color, &out.rainbow);
      JsonVariantConst bg = o["background"];
      if (bg.isNull()) bg = o["bg"];
      if (!bg.isNull()) readColor(bg, out.background, nullptr);
      if (o["effect"].is<const char*>()) parseEffect(o["effect"].as<const char*>(), out.effect);
      double d;
      if (readNumber(o["speed"], d) && d > 0) readClamped(o["speed"], 5, 120, out.speed);
      if (readNumber(o["duration"], d) && d > 0) readClamped(o["duration"], 1, kMaxDurationS, out.durationS);
      readClamped(o["repeat"], 0, 20, out.repeat);
      readClamped(o["priority"], -100, 100, out.priority);
      if (readNumber(o["brightness"], d) && d > 0) readClamped(o["brightness"], 1, 100, out.brightness);
      readBool(o["once"], out.once);
      if (!o["expires"].isNull() && !readExpiry(o["expires"], out.expires)) {
        error = "can't read \"expires\" (use unix seconds or an ISO-8601 time)";
        return false;
      }
      if (o["when"].is<const char*>()) parseWhen(o["when"].as<const char*>(), out.when);
      if (o["icon"].is<const char*>()) {
        std::string icon(trim(o["icon"].as<const char*>()));
        for (char& c : icon) {
          if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        if (!icon.empty() && findIconByName(icon) >= 0) out.text = ":" + icon + ": " + out.text;
      }
    }
  }

  truncateUtf8(out.text, kMaxTextBytes);
  if (trim(out.text).empty()) {
    error = "empty text";
    return false;
  }
  return true;
}

}  // namespace lb
