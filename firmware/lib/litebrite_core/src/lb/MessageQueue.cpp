#include "lb/MessageQueue.h"

#include <algorithm>

namespace lb {

namespace {

bool isExpired(const Message& m, int64_t now) { return now > 0 && m.expires > 0 && m.expires <= now; }

// True if a should be evicted before b.
bool evictsBefore(const Message& a, const Message& b) {
  if (a.priority != b.priority) return a.priority < b.priority;
  return a.seq < b.seq;
}

}  // namespace

bool MessageQueue::upsert(Message msg) {
  msg.seq = nextSeq_++;
  for (auto& existing : items_) {
    if (existing.slot == msg.slot) {
      existing = std::move(msg);
      return true;
    }
  }
  if (items_.size() >= kCapacity) {
    auto victim = std::min_element(items_.begin(), items_.end(), evictsBefore);
    if (msg.priority <= victim->priority) return false;
    items_.erase(victim);
  }
  items_.push_back(std::move(msg));
  return true;
}

bool MessageQueue::remove(std::string_view slot) {
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    if (it->slot == slot) {
      items_.erase(it);
      return true;
    }
  }
  return false;
}

const Message* MessageQueue::find(std::string_view slot) const {
  for (const auto& m : items_) {
    if (m.slot == slot) return &m;
  }
  return nullptr;
}

std::vector<const Message*> MessageQueue::ordered(int64_t now) const {
  std::vector<const Message*> out;
  out.reserve(items_.size());
  for (const auto& m : items_) {
    if (!isExpired(m, now)) out.push_back(&m);
  }
  std::stable_sort(out.begin(), out.end(), [](const Message* a, const Message* b) {
    if (a->priority != b->priority) return a->priority > b->priority;
    return a->seq < b->seq;
  });
  return out;
}

std::vector<std::string> MessageQueue::expired(int64_t now) const {
  std::vector<std::string> out;
  for (const auto& m : items_) {
    if (isExpired(m, now)) out.push_back(m.slot);
  }
  return out;
}

}  // namespace lb
