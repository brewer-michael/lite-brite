#include "lb/Player.h"

#include <algorithm>
#include <cmath>

#include "lb/Render.h"

namespace lb {

namespace {

constexpr uint32_t kMaxFrameUs = 50000;     // at least 20 fps
constexpr uint32_t kStaticFrameUs = 25000;  // 40 fps for blink/pulse/rainbow
constexpr uint32_t kBlinkPeriodUs = 900000;
constexpr uint32_t kBlinkOnUs = 600000;
constexpr uint32_t kPulsePeriodUs = 2000000;
constexpr uint32_t kRainbowUsPerDegree = 8000;

}  // namespace

void MessagePlayer::start(const Message& msg, const PlayDefaults& defaults, int canvasWidth,
                          int canvasHeight) {
  msg_ = msg;
  line_.set(msg.text);
  canvasW_ = canvasWidth;
  const int scale = std::max(1, canvasHeight / 8);
  textW_ = line_.width() * scale;
  brightness_ = msg.brightness > 0 ? msg.brightness : defaults.brightness;

  const uint32_t speed = std::max<uint32_t>(1, msg.speed > 0 ? msg.speed : defaults.speed);
  const uint64_t durationUs =
      std::min<uint64_t>(uint64_t(msg.durationS > 0 ? msg.durationS : defaults.durationS) * 1000000u,
                         kMaxShowUs);
  scrolls_ = textW_ > canvasW_;
  introUs_ = msg.effect == Effect::Flash ? kFlashCount * 2 * kFlashHalfUs : 0;

  // One pixel of scroll per step. Frames land on an exact divisor of the step
  // so every step is the same number of frames: no judder.
  uint32_t step = (1000000u + speed - 1) / speed;
  const uint32_t framesPerStep = std::max<uint32_t>(1, (step + kMaxFrameUs - 1) / kMaxFrameUs);
  step = ((step + framesPerStep - 1) / framesPerStep) * framesPerStep;
  stepUs_ = step;

  if (scrolls_) {
    frameUs_ = step / framesPerStep;
    holdUs_ = std::max(kLeadHoldUs, introUs_);
    firstPassUs_ = holdUs_ + static_cast<uint32_t>(textW_) * step;
    passGapUs_ = msg.effect == Effect::Flash ? 2 * kFlashHalfUs : 0;
    laterPassUs_ = passGapUs_ + static_cast<uint32_t>(canvasW_ + textW_) * step;
    if (msg.repeat > 0) {
      passes_ = msg.repeat;
    } else if (durationUs <= firstPassUs_) {
      passes_ = 1;
    } else {
      passes_ = 1 + static_cast<uint32_t>((durationUs - firstPassUs_ + laterPassUs_ - 1) / laterPassUs_);
    }
    uint64_t total = firstPassUs_ + uint64_t(passes_ - 1) * laterPassUs_;
    while (passes_ > 1 && total > kMaxShowUs) {
      --passes_;
      total -= laterPassUs_;
    }
    totalUs_ = static_cast<uint32_t>(std::min<uint64_t>(total, kMaxShowUs));
  } else {
    frameUs_ = kStaticFrameUs;
    holdUs_ = firstPassUs_ = passGapUs_ = laterPassUs_ = 0;
    passes_ = 1;
    uint64_t total = msg.repeat > 0 ? uint64_t(msg.repeat) * kStaticPassUs : durationUs;
    total = std::max<uint64_t>(total, introUs_ + 1000000u);
    totalUs_ = static_cast<uint32_t>(std::min<uint64_t>(total, kMaxShowUs));
  }
}

bool MessagePlayer::render(Canvas& canvas, uint32_t tUs) const {
  if (tUs >= totalUs_) {
    canvas.clear();
    return false;
  }
  canvas.clear(msg_.background);

  TextStyle style;
  style.color = msg_.color;
  style.rainbow = msg_.rainbow;
  style.rainbowPhase = static_cast<int>(tUs / kRainbowUsPerDegree);

  if (msg_.effect == Effect::Blink && tUs % kBlinkPeriodUs >= kBlinkOnUs) return true;
  if (msg_.effect == Effect::Pulse) {
    const float phase = static_cast<float>(tUs % kPulsePeriodUs) / kPulsePeriodUs;
    const float wave = 0.5f - 0.5f * std::cos(phase * 6.2831853f);
    style.level = static_cast<uint8_t>(70 + 185 * wave);
  }

  int x;
  bool inverse = false;
  if (!scrolls_) {
    x = (canvasW_ - textW_) / 2;
    inverse = tUs < introUs_ && (tUs / kFlashHalfUs) % 2 == 0;
  } else if (tUs < firstPassUs_) {
    inverse = tUs < introUs_ && (tUs / kFlashHalfUs) % 2 == 0;
    x = tUs < holdUs_ ? 0 : -static_cast<int>((tUs - holdUs_) / stepUs_);
  } else {
    const uint32_t inPass = (tUs - firstPassUs_) % laterPassUs_;
    if (inPass < passGapUs_) {
      inverse = inPass < kFlashHalfUs;
      x = canvasW_;
    } else {
      x = canvasW_ - static_cast<int>((inPass - passGapUs_) / stepUs_);
    }
  }

  if (inverse) {
    canvas.clear(msg_.rainbow ? hsv(style.rainbowPhase, 255, 255) : msg_.color);
    style.inverse = true;
  }
  drawText(canvas, line_, x, style);
  return true;
}

void Playlist::configure(const PlayDefaults& defaults, int canvasWidth, int canvasHeight) {
  defaults_ = defaults;
  width_ = canvasWidth;
  height_ = canvasHeight;
}

void Playlist::add(const Message& msg) {
  if (playing_ && player_.message().slot == msg.slot) {
    // Home Assistant changed the message on screen: show the new version now.
    player_.start(msg, defaults_, width_, height_);
    restart_ = true;
    return;
  }
  for (auto& queued : queue_) {
    if (queued.slot == msg.slot) {
      queued = msg;
      return;
    }
  }
  queue_.push_back(msg);
}

bool Playlist::remove(std::string_view slot) {
  bool removed = false;
  for (auto it = queue_.begin(); it != queue_.end();) {
    if (it->slot == slot) {
      it = queue_.erase(it);
      removed = true;
    } else {
      ++it;
    }
  }
  if (playing_ && player_.message().slot == slot) {
    playing_ = false;
    inGap_ = false;
    removed = true;
  }
  return removed;
}

bool Playlist::contains(std::string_view slot) const {
  if (playing_ && player_.message().slot == slot) return true;
  for (const auto& queued : queue_) {
    if (queued.slot == slot) return true;
  }
  return false;
}

void Playlist::clear() {
  queue_.clear();
  playing_ = false;
  inGap_ = false;
  restart_ = false;
}

bool Playlist::update(Canvas& canvas, uint32_t nowUs, Message* finished) {
  if (!playing_) {
    if (inGap_ && nowUs - gapStartUs_ < kMessageGapUs) {
      canvas.clear();
      return false;
    }
    inGap_ = false;
    if (queue_.empty()) {
      canvas.clear();
      return false;
    }
    player_.start(queue_.front(), defaults_, width_, height_);
    queue_.pop_front();
    playing_ = true;
    startUs_ = nowUs;
  } else if (restart_) {
    startUs_ = nowUs;
  }
  restart_ = false;

  if (player_.render(canvas, nowUs - startUs_)) return false;

  playing_ = false;
  inGap_ = true;
  gapStartUs_ = nowUs;
  if (finished != nullptr) *finished = player_.message();
  return true;
}

}  // namespace lb
