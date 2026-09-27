#pragma once

#include <cstdint>
#include <deque>
#include <string_view>

#include "lb/Canvas.h"
#include "lb/Message.h"
#include "lb/Text.h"

namespace lb {

// Device-wide defaults a message can override.
struct PlayDefaults {
  uint16_t speed = 24;       // px/s
  uint16_t durationS = 30;   // seconds per message
  uint8_t brightness = 50;   // percent
};

// Timeline constants, exposed for tests and docs.
constexpr uint32_t kFlashHalfUs = 150000;    // one on/off half of a flash
constexpr uint32_t kFlashCount = 3;          // flashes before the first pass
constexpr uint32_t kLeadHoldUs = 1000000;    // long text starts left-aligned and holds this long
constexpr uint32_t kStaticPassUs = 3000000;  // one "pass" of text that fits (for repeat)
constexpr uint32_t kMaxShowUs = 600000000;   // hard cap per message: 10 minutes
constexpr uint32_t kMessageGapUs = 400000;   // blank pause between messages

// Plays one message: lays out the text once, then renders any point on its
// timeline. Rendering is a pure function of time so the firmware and the
// simulator draw identical frames.
//
// Long text starts left-aligned so the first words are readable immediately,
// holds for a moment, scrolls off, then re-enters from the right for further
// passes. Text that fits is centered and doesn't move. "flash" adds three
// inverse-video flashes at the start and one before each later pass.
class MessagePlayer {
 public:
  void start(const Message& msg, const PlayDefaults& defaults, int canvasWidth, int canvasHeight);

  // Clears the canvas and draws the frame `tUs` microseconds after start.
  // Returns false (leaving the canvas blank) once the message has finished.
  bool render(Canvas& canvas, uint32_t tUs) const;

  const Message& message() const { return msg_; }
  uint32_t durationUs() const { return totalUs_; }
  uint32_t frameIntervalUs() const { return frameUs_; }
  uint8_t brightness() const { return brightness_; }
  bool scrolls() const { return scrolls_; }
  int textWidth() const { return textW_; }
  uint32_t passes() const { return passes_; }

 private:
  Message msg_;
  TextLine line_;
  int canvasW_ = 0;
  int textW_ = 0;  // canvas pixels
  bool scrolls_ = false;
  uint8_t brightness_ = 50;
  uint32_t stepUs_ = 0;      // time per pixel of scroll
  uint32_t introUs_ = 0;     // flash intro length
  uint32_t holdUs_ = 0;      // left-aligned hold at the start of pass 1
  uint32_t firstPassUs_ = 0;
  uint32_t passGapUs_ = 0;   // flash between later passes
  uint32_t laterPassUs_ = 0;
  uint32_t passes_ = 1;
  uint32_t totalUs_ = 0;
  uint32_t frameUs_ = 40000;
};

// Plays a list of messages back to back with a short blank gap between them.
// Messages can be added or withdrawn while it's running (for example when
// Home Assistant clears a message mid-show).
class Playlist {
 public:
  void configure(const PlayDefaults& defaults, int canvasWidth, int canvasHeight);

  // Queues a message, or refreshes the queued copy with the same slot. A new
  // version of the message that's on screen replaces it immediately.
  void add(const Message& msg);

  // Drops the slot from the queue and stops it if it's playing. Returns true
  // if anything was removed.
  bool remove(std::string_view slot);

  bool contains(std::string_view slot) const;
  bool active() const { return playing_ || !queue_.empty(); }
  void clear();

  // Draws the frame for `nowUs` (any monotonically increasing microsecond
  // clock; wrap-around is fine). When a message finishes playing during this
  // call, copies it to *finished (if non-null) and returns true.
  bool update(Canvas& canvas, uint32_t nowUs, Message* finished);

  // Suggested time until the next frame, for even scroll steps.
  uint32_t frameIntervalUs() const { return playing_ ? player_.frameIntervalUs() : 40000; }
  uint8_t brightness() const { return player_.brightness(); }
  const Message* current() const { return playing_ ? &player_.message() : nullptr; }

 private:
  PlayDefaults defaults_;
  int width_ = 32;
  int height_ = 8;
  std::deque<Message> queue_;
  MessagePlayer player_;
  bool playing_ = false;
  bool inGap_ = false;
  bool restart_ = false;  // the playing message was replaced; restart its clock
  uint32_t startUs_ = 0;
  uint32_t gapStartUs_ = 0;
};

}  // namespace lb
