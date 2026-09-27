#include "lb/Topics.h"

namespace lb {

namespace {

bool startsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

bool isValidName(std::string_view name) {
  if (name.empty() || name.size() > 32) return false;
  for (char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

Topics::Topics(std::string_view prefix, std::string_view deviceId)
    : deviceId_(deviceId), base_(std::string(prefix) + "/" + std::string(deviceId)) {
  uniqueId_ = "lite_brite_";
  for (char c : deviceId) {
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (c >= 'A' && c <= 'Z') {
      uniqueId_ += static_cast<char>(c - 'A' + 'a');
    } else {
      uniqueId_ += alnum ? c : '_';
    }
  }
}

std::string Topics::discovery(std::string_view discoveryPrefix) const {
  return std::string(discoveryPrefix) + "/device/" + uniqueId_ + "/config";
}

Topics::Kind Topics::classify(std::string_view topic, std::string_view& rest) const {
  rest = std::string_view();
  if (!startsWith(topic, base_) || topic.size() <= base_.size() || topic[base_.size()] != '/') {
    return Kind::Unknown;
  }
  const std::string_view tail = topic.substr(base_.size() + 1);
  if (startsWith(tail, "msg/")) {
    rest = tail.substr(4);
    return isValidName(rest) ? Kind::Message : Kind::Unknown;
  }
  if (startsWith(tail, "config/")) {
    rest = tail.substr(7);
    return rest.empty() ? Kind::Unknown : Kind::Setting;
  }
  if (tail == "cmd") return Kind::Command;
  if (tail == "sync") return Kind::Sync;
  return Kind::Unknown;
}

}  // namespace lb
