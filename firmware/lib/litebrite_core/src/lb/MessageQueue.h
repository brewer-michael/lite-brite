#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "lb/Message.h"

namespace lb {

// The messages currently waiting to be shown, one per slot. The MQTT broker's
// retained messages are the source of truth; this is the sign's working copy
// for one wake.
class MessageQueue {
 public:
  static constexpr size_t kCapacity = 12;

  // Adds a message or replaces the one in the same slot. When the queue is
  // full, the lowest-priority (then oldest) message is evicted to make room,
  // unless the new message would itself be the lowest. Returns false if the
  // message was rejected. Assigns msg.seq.
  bool upsert(Message msg);
  bool remove(std::string_view slot);
  void clear() { items_.clear(); }

  size_t size() const { return items_.size(); }
  bool empty() const { return items_.empty(); }
  const Message* find(std::string_view slot) const;
  const std::vector<Message>& items() const { return items_; }

  // Waiting messages in play order: priority (high first), then arrival.
  // Messages that have expired are skipped when `now` (unix seconds) is known;
  // pass 0 when the clock isn't set.
  std::vector<const Message*> ordered(int64_t now) const;

  // Slots whose `expires` time has passed. Empty when `now` is 0 (unknown).
  std::vector<std::string> expired(int64_t now) const;

 private:
  std::vector<Message> items_;
  uint32_t nextSeq_ = 1;
};

}  // namespace lb
