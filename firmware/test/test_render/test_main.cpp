#include <unity.h>

#include <string>
#include <vector>

#include "lb/Canvas.h"
#include "lb/Output.h"
#include "lb/Player.h"
#include "lb/Render.h"

using namespace lb;

void setUp() {}
void tearDown() {}

static int litPixels(const Canvas& c) {
  int n = 0;
  for (int y = 0; y < c.height(); ++y) {
    for (int x = 0; x < c.width(); ++x) n += c.get(x, y).isBlack() ? 0 : 1;
  }
  return n;
}

static int leftmostLitColumn(const Canvas& c) {
  for (int x = 0; x < c.width(); ++x) {
    for (int y = 0; y < c.height(); ++y) {
      if (!c.get(x, y).isBlack()) return x;
    }
  }
  return -1;
}

static Message msg(const char* text, Effect effect = Effect::Scroll) {
  Message m;
  m.slot = "test";
  m.text = text;
  m.color = Rgb(255, 128, 0);
  m.effect = effect;
  return m;
}

// ---- drawing ----------------------------------------------------------------

void test_draw_text_pixels() {
  Canvas c(32, 8);
  TextLine line;
  line.set("I");  // ###/.#./.#./.#./.#./.#./###
  drawText(c, line, 0, TextStyle{});
  TEST_ASSERT_EQUAL(3 + 5 + 3, litPixels(c));
  TEST_ASSERT_FALSE(c.get(0, 0).isBlack());
  TEST_ASSERT_TRUE(c.get(0, 1).isBlack());
  TEST_ASSERT_FALSE(c.get(1, 3).isBlack());
  TEST_ASSERT_TRUE(c.get(0, 7).isBlack());
}

void test_draw_text_clips_and_offsets() {
  Canvas c(32, 8);
  TextLine line;
  line.set("I");
  drawText(c, line, -2, TextStyle{});  // only the right column of "I" is visible
  TEST_ASSERT_EQUAL(2, litPixels(c));
  c.clear();
  drawText(c, line, 31, TextStyle{});
  TEST_ASSERT_EQUAL(2, litPixels(c));
  c.clear();
  drawText(c, line, 40, TextStyle{});
  TEST_ASSERT_EQUAL(0, litPixels(c));
}

void test_text_scales_on_taller_matrices() {
  Canvas c(64, 16);
  TEST_ASSERT_EQUAL(2, textScale(c));
  TEST_ASSERT_EQUAL(0, textTop(c));
  TextLine line;
  line.set("I");
  drawText(c, line, 0, TextStyle{});
  TEST_ASSERT_EQUAL((3 + 5 + 3) * 4, litPixels(c));
  Canvas odd(32, 10);
  TEST_ASSERT_EQUAL(1, textScale(odd));
  TEST_ASSERT_EQUAL(1, textTop(odd));
}

void test_icons_use_their_palette_and_text_color() {
  Canvas c(32, 8);
  TextLine line;
  line.set(":heart::right:");
  TextStyle style;
  style.color = Rgb(0, 0, 255);
  drawText(c, line, 0, style);
  TEST_ASSERT_TRUE(c.get(1, 1) == Rgb(255, 0, 0));        // heart is red
  TEST_ASSERT_TRUE(c.get(8 + 3, 3) == Rgb(0, 0, 255));    // arrow takes the text color
}

void test_inverse_draws_black() {
  Canvas c(32, 8);
  c.clear(Rgb(255, 0, 0));
  TextLine line;
  line.set("I");
  TextStyle style;
  style.inverse = true;
  drawText(c, line, 0, style);
  TEST_ASSERT_TRUE(c.get(0, 0).isBlack());
  TEST_ASSERT_FALSE(c.get(0, 1).isBlack());
}

// ---- MessagePlayer ---------------------------------------------------------

void test_short_text_is_centered_and_static() {
  MessagePlayer p;
  PlayDefaults d;
  p.start(msg("Hi"), d, 32, 8);
  TEST_ASSERT_FALSE(p.scrolls());
  TEST_ASSERT_EQUAL_UINT32(30u * 1000000u, p.durationUs());
  Canvas c(32, 8);
  TEST_ASSERT_TRUE(p.render(c, 0));
  const int left = leftmostLitColumn(c);
  TEST_ASSERT_EQUAL((32 - 7) / 2, left);
  Canvas later(32, 8);
  TEST_ASSERT_TRUE(p.render(later, 20000000));
  TEST_ASSERT_EQUAL(left, leftmostLitColumn(later));
  TEST_ASSERT_FALSE(p.render(c, p.durationUs()));
  TEST_ASSERT_EQUAL(0, litPixels(c));
}

void test_long_text_starts_left_aligned_then_scrolls() {
  MessagePlayer p;
  PlayDefaults d;
  d.speed = 25;  // 40 ms per pixel
  p.start(msg("Welcome home! Keys away first"), d, 32, 8);
  TEST_ASSERT_TRUE(p.scrolls());
  Canvas c(32, 8);
  p.render(c, 0);
  TEST_ASSERT_EQUAL(0, leftmostLitColumn(c));  // 'W' starts at column 0
  Canvas held(32, 8);
  p.render(held, kLeadHoldUs - 1);
  for (int i = 0; i < 32 * 8; ++i) TEST_ASSERT_TRUE(held.data()[i] == c.data()[i]);
  // One pixel of scroll per 40 ms after the hold.
  Canvas moved(32, 8);
  p.render(moved, kLeadHoldUs + 40000);
  TEST_ASSERT_TRUE(moved.get(0, 0) == c.get(1, 0));
  TEST_ASSERT_TRUE(moved.get(5, 3) == c.get(6, 3));
}

void test_scroll_passes_cover_the_duration() {
  MessagePlayer p;
  PlayDefaults d;
  d.speed = 20;
  d.durationS = 30;
  p.start(msg("Welcome home! Keys away first"), d, 32, 8);
  const uint32_t step = 50000;
  const uint32_t textW = static_cast<uint32_t>(p.textWidth());
  const uint32_t first = kLeadHoldUs + textW * step;
  const uint32_t later = (32 + textW) * step;
  TEST_ASSERT_TRUE(p.passes() >= 2);
  TEST_ASSERT_EQUAL_UINT32(first + (p.passes() - 1) * later, p.durationUs());
  TEST_ASSERT_TRUE(p.durationUs() >= 30u * 1000000u);
  TEST_ASSERT_TRUE(p.durationUs() - later < 30u * 1000000u);  // no extra pass
}

void test_repeat_overrides_duration() {
  MessagePlayer p;
  Message m = msg("Welcome home! Keys away first");
  m.repeat = 1;
  m.durationS = 300;
  p.start(m, PlayDefaults{}, 32, 8);
  TEST_ASSERT_EQUAL_UINT32(1, p.passes());
}

void test_later_passes_enter_from_the_right() {
  MessagePlayer p;
  PlayDefaults d;
  d.speed = 25;
  p.start(msg("Welcome home! Keys away first"), d, 32, 8);
  const uint32_t first = kLeadHoldUs + static_cast<uint32_t>(p.textWidth()) * 40000;
  Canvas c(32, 8);
  p.render(c, first);  // text just off the right edge
  TEST_ASSERT_EQUAL(0, litPixels(c));
  p.render(c, first + 40000);  // first column of 'W' at x = 31
  TEST_ASSERT_EQUAL(31, leftmostLitColumn(c));
}

void test_frame_interval_divides_the_step() {
  for (uint16_t speed : {5, 7, 13, 19, 20, 24, 25, 33, 60, 120}) {
    MessagePlayer p;
    PlayDefaults d;
    d.speed = speed;
    p.start(msg("A long message that certainly needs to scroll"), d, 32, 8);
    const uint32_t f = p.frameIntervalUs();
    TEST_ASSERT_TRUE(f <= 50000);
    // Sample at frame times: the scroll must advance at most one pixel per frame
    // and by the same number of frames each step.
    Canvas a(32, 8), b(32, 8);
    int lastShiftFrame = -1;
    int gap = -1;
    for (int k = 0; k < 200; ++k) {
      const uint32_t t = kLeadHoldUs + static_cast<uint32_t>(k) * f;
      p.render(a, t);
      p.render(b, t + f);
      bool same = true;
      for (int i = 0; i < 32 * 8; ++i) same = same && a.data()[i] == b.data()[i];
      if (!same) {
        if (lastShiftFrame >= 0) {
          if (gap < 0) gap = k - lastShiftFrame;
          TEST_ASSERT_EQUAL_MESSAGE(gap, k - lastShiftFrame, "uneven scroll step");
        }
        lastShiftFrame = k;
      }
    }
  }
}

void test_ticker_scrolls_short_text_in_from_the_right() {
  MessagePlayer p;
  PlayDefaults d;
  d.speed = 25;
  d.durationS = 1;
  p.start(msg("Hi", Effect::Ticker), d, 32, 8);
  TEST_ASSERT_TRUE(p.scrolls());
  TEST_ASSERT_EQUAL_UINT32(1, p.passes());
  TEST_ASSERT_EQUAL_UINT32((32 + 7) * 40000u, p.durationUs());
  Canvas c(32, 8);
  p.render(c, 0);
  TEST_ASSERT_EQUAL(0, litPixels(c));
  p.render(c, 40000);
  TEST_ASSERT_EQUAL(31, leftmostLitColumn(c));
  p.render(c, 32 * 40000u);
  TEST_ASSERT_EQUAL(0, leftmostLitColumn(c));
}

void test_flash_intro_inverts() {
  MessagePlayer p;
  p.start(msg("Keys!", Effect::Flash), PlayDefaults{}, 32, 8);
  Canvas c(32, 8);
  p.render(c, 0);  // first half of first flash: filled with the text color
  TEST_ASSERT_TRUE(litPixels(c) > 32 * 8 / 2);
  TEST_ASSERT_TRUE(c.get(0, 0) == Rgb(255, 128, 0));
  p.render(c, kFlashHalfUs);  // second half: normal text
  TEST_ASSERT_TRUE(litPixels(c) < 32 * 8 / 2);
  p.render(c, kFlashCount * 2 * kFlashHalfUs + 1);  // after the intro
  TEST_ASSERT_TRUE(litPixels(c) < 32 * 8 / 2);
}

void test_blink_and_pulse() {
  MessagePlayer p;
  p.start(msg("Hi", Effect::Blink), PlayDefaults{}, 32, 8);
  Canvas c(32, 8);
  p.render(c, 100000);
  TEST_ASSERT_TRUE(litPixels(c) > 0);
  p.render(c, 700000);
  TEST_ASSERT_EQUAL(0, litPixels(c));

  p.start(msg("Hi", Effect::Pulse), PlayDefaults{}, 32, 8);
  Canvas dim(32, 8), bright(32, 8);
  p.render(dim, 0);
  p.render(bright, 1000000);
  const int x = leftmostLitColumn(bright);
  TEST_ASSERT_TRUE(bright.get(x, 0).r > dim.get(x, 0).r);
}

void test_background_color() {
  MessagePlayer p;
  Message m = msg("Hi");
  m.background = Rgb(0, 0, 50);
  p.start(m, PlayDefaults{}, 32, 8);
  Canvas c(32, 8);
  p.render(c, 0);
  TEST_ASSERT_TRUE(c.get(0, 7) == Rgb(0, 0, 50));
}

void test_brightness_override() {
  MessagePlayer p;
  PlayDefaults d;
  d.brightness = 40;
  p.start(msg("Hi"), d, 32, 8);
  TEST_ASSERT_EQUAL_UINT8(40, p.brightness());
  Message m = msg("Hi");
  m.brightness = 90;
  p.start(m, d, 32, 8);
  TEST_ASSERT_EQUAL_UINT8(90, p.brightness());
}

// ---- Playlist --------------------------------------------------------------

void test_playlist_plays_in_order_with_gap() {
  Playlist pl;
  PlayDefaults d;
  d.durationS = 2;
  pl.configure(d, 32, 8);
  pl.add(msg("A"));
  Message b = msg("B");
  b.slot = "b";
  pl.add(b);
  Canvas c(32, 8);
  Message done;
  std::vector<std::string> finished;
  uint32_t t = 0;
  for (; t < 10000000 && pl.active(); t += 20000) {
    if (pl.update(c, t, &done)) finished.push_back(done.slot);
  }
  TEST_ASSERT_EQUAL(2, finished.size());
  TEST_ASSERT_EQUAL_STRING("test", finished[0].c_str());
  TEST_ASSERT_EQUAL_STRING("b", finished[1].c_str());
  TEST_ASSERT_TRUE(t >= 4000000 + kMessageGapUs);
}

void test_playlist_remove_stops_current() {
  Playlist pl;
  pl.configure(PlayDefaults{}, 32, 8);
  pl.add(msg("A"));
  Canvas c(32, 8);
  Message done;
  TEST_ASSERT_FALSE(pl.update(c, 0, &done));
  TEST_ASSERT_TRUE(pl.contains("test"));
  TEST_ASSERT_TRUE(pl.remove("test"));
  TEST_ASSERT_FALSE(pl.active());
  TEST_ASSERT_FALSE(pl.update(c, 20000, &done));
  TEST_ASSERT_EQUAL(0, litPixels(c));
}

void test_playlist_replacing_current_restarts_it() {
  Playlist pl;
  PlayDefaults d;
  d.durationS = 2;
  pl.configure(d, 32, 8);
  pl.add(msg("A"));
  Canvas c(32, 8);
  Message done;
  pl.update(c, 0, &done);
  pl.update(c, 1500000, &done);
  pl.add(msg("B"));  // same slot, new text
  bool finished = false;
  uint32_t t = 1520000;
  for (; t < 10000000 && !finished; t += 20000) finished = pl.update(c, t, &done);
  TEST_ASSERT_TRUE(finished);
  TEST_ASSERT_EQUAL_STRING("B", done.text.c_str());
  TEST_ASSERT_TRUE(t >= 1520000 + 2000000);  // full duration from the restart
}

void test_playlist_remove_queued_and_gap() {
  Playlist pl;
  PlayDefaults d;
  d.durationS = 1;
  pl.configure(d, 32, 8);
  Message a = msg("A");
  a.slot = "a";
  Message b = msg("B");
  b.slot = "b";
  pl.add(a);
  pl.add(b);
  TEST_ASSERT_TRUE(pl.remove("b"));  // still queued
  TEST_ASSERT_FALSE(pl.contains("b"));
  TEST_ASSERT_FALSE(pl.remove("nope"));
  Canvas c(32, 8);
  Message done;
  uint32_t t = 0;
  bool finished = false;
  for (; t < 5000000 && !finished; t += 20000) finished = pl.update(c, t, &done);
  TEST_ASSERT_TRUE(finished);
  TEST_ASSERT_FALSE(pl.active());
  // A message added during the gap waits for the gap to end before it starts.
  pl.add(b);
  TEST_ASSERT_FALSE(pl.update(c, t, &done));
  TEST_ASSERT_EQUAL(0, litPixels(c));
  TEST_ASSERT_NULL(pl.current());
  pl.update(c, t + kMessageGapUs, &done);
  TEST_ASSERT_NOT_NULL(pl.current());
  TEST_ASSERT_EQUAL_STRING("b", pl.current()->slot.c_str());
  TEST_ASSERT_TRUE(litPixels(c) > 0);
}

void test_playlist_refreshes_a_queued_copy() {
  Playlist pl;
  pl.configure(PlayDefaults{}, 32, 8);
  Message a = msg("A");
  a.slot = "a";
  Message b = msg("old");
  b.slot = "b";
  pl.add(a);
  pl.add(b);
  b.text = "new";
  pl.add(b);  // replaces the queued copy instead of queueing twice
  Canvas c(32, 8);
  Message done;
  std::vector<std::string> texts;
  for (uint32_t t = 0; t < 100000000 && pl.active(); t += 50000) {
    if (pl.update(c, t, &done)) texts.push_back(done.text);
  }
  TEST_ASSERT_EQUAL(2, texts.size());
  TEST_ASSERT_EQUAL_STRING("new", texts[1].c_str());
}

// ---- Output ----------------------------------------------------------------

void test_layout_column_major_serpentine() {
  MatrixLayout l;  // 32x8 column-major serpentine
  TEST_ASSERT_EQUAL(0, l.index(0, 0));
  TEST_ASSERT_EQUAL(7, l.index(0, 7));
  TEST_ASSERT_EQUAL(8, l.index(1, 7));
  TEST_ASSERT_EQUAL(15, l.index(1, 0));
  TEST_ASSERT_EQUAL(16, l.index(2, 0));
  TEST_ASSERT_EQUAL(255, l.index(31, 0));
  l.flipX = true;
  TEST_ASSERT_EQUAL(255, l.index(0, 0));
}

void test_layout_row_major() {
  MatrixLayout l;
  l.width = 16;
  l.height = 16;
  l.columnMajor = false;
  TEST_ASSERT_EQUAL(15, l.index(15, 0));
  TEST_ASSERT_EQUAL(16, l.index(15, 1));
  TEST_ASSERT_EQUAL(31, l.index(0, 1));
  l.serpentine = false;
  TEST_ASSERT_EQUAL(16, l.index(0, 1));
  l.flipY = true;
  TEST_ASSERT_EQUAL(15 * 16, l.index(0, 0));
}

void test_brightness_curve() {
  TEST_ASSERT_EQUAL_UINT8(0, brightnessLevel(0));
  TEST_ASSERT_EQUAL_UINT8(1, brightnessLevel(1));
  TEST_ASSERT_EQUAL_UINT8(55, brightnessLevel(50));
  TEST_ASSERT_EQUAL_UINT8(255, brightnessLevel(100));
  TEST_ASSERT_EQUAL_UINT8(255, brightnessLevel(150));
}

void test_output_gamma_order_and_brightness() {
  OutputStage out;
  TEST_ASSERT_EQUAL_UINT8(0, out.gamma(0));
  TEST_ASSERT_EQUAL_UINT8(255, out.gamma(255));
  TEST_ASSERT_EQUAL_UINT8(42, out.gamma(128));
  MatrixLayout l;
  Canvas c(32, 8);
  c.set(0, 0, Rgb(255, 128, 0));
  std::vector<uint8_t> wire(l.count() * 3);
  LedPowerModel model;
  out.render(c, l, ColorOrder::GRB, 255, 100000, model, wire.data());
  TEST_ASSERT_EQUAL_UINT8(42, wire[0]);   // G first
  TEST_ASSERT_EQUAL_UINT8(255, wire[1]);  // then R
  TEST_ASSERT_EQUAL_UINT8(0, wire[2]);
  out.render(c, l, ColorOrder::RGB, 128, 100000, model, wire.data());
  TEST_ASSERT_EQUAL_UINT8(128, wire[0]);
  TEST_ASSERT_EQUAL_UINT8(21, wire[1]);
}

void test_power_limit() {
  OutputStage out;
  MatrixLayout l;
  Canvas c(32, 8);
  c.clear(colors::kWhite);
  std::vector<uint8_t> wire(l.count() * 3);
  LedPowerModel model;  // 16 mA per channel, 0.7 mA idle per LED
  const uint32_t unlimited = out.render(c, l, ColorOrder::GRB, 255, 100000, model, wire.data());
  TEST_ASSERT_EQUAL_UINT32(256 * 48 + 179, unlimited);
  const uint32_t limited = out.render(c, l, ColorOrder::GRB, 255, 1200, model, wire.data());
  TEST_ASSERT_TRUE(limited <= 1200);
  TEST_ASSERT_TRUE(limited > 1100);
  TEST_ASSERT_EQUAL_UINT32(limited, estimateMilliamps(wire.data(), l.count(), model));
  const uint32_t none = out.render(c, l, ColorOrder::GRB, 255, 100, model, wire.data());
  TEST_ASSERT_EQUAL_UINT32(179, none);  // below the idle draw: everything dark
  TEST_ASSERT_EQUAL_UINT8(0, wire[0]);
}

void test_wiring_pattern_corners() {
  Canvas c(32, 8);
  drawWiringTest(c, 0);
  TEST_ASSERT_TRUE(c.get(0, 0) == colors::kRed);
  TEST_ASSERT_TRUE(c.get(31, 0) == colors::kGreen);
  TEST_ASSERT_TRUE(c.get(0, 7) == Rgb(0, 0, 255));
  drawWiringTest(c, 60 * 33);
  TEST_ASSERT_TRUE(c.get(1, 1) == colors::kWhite);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_draw_text_pixels);
  RUN_TEST(test_draw_text_clips_and_offsets);
  RUN_TEST(test_text_scales_on_taller_matrices);
  RUN_TEST(test_icons_use_their_palette_and_text_color);
  RUN_TEST(test_inverse_draws_black);
  RUN_TEST(test_short_text_is_centered_and_static);
  RUN_TEST(test_long_text_starts_left_aligned_then_scrolls);
  RUN_TEST(test_scroll_passes_cover_the_duration);
  RUN_TEST(test_repeat_overrides_duration);
  RUN_TEST(test_later_passes_enter_from_the_right);
  RUN_TEST(test_frame_interval_divides_the_step);
  RUN_TEST(test_ticker_scrolls_short_text_in_from_the_right);
  RUN_TEST(test_flash_intro_inverts);
  RUN_TEST(test_blink_and_pulse);
  RUN_TEST(test_background_color);
  RUN_TEST(test_brightness_override);
  RUN_TEST(test_playlist_plays_in_order_with_gap);
  RUN_TEST(test_playlist_remove_stops_current);
  RUN_TEST(test_playlist_replacing_current_restarts_it);
  RUN_TEST(test_playlist_remove_queued_and_gap);
  RUN_TEST(test_playlist_refreshes_a_queued_copy);
  RUN_TEST(test_layout_column_major_serpentine);
  RUN_TEST(test_layout_row_major);
  RUN_TEST(test_brightness_curve);
  RUN_TEST(test_output_gamma_order_and_brightness);
  RUN_TEST(test_power_limit);
  RUN_TEST(test_wiring_pattern_corners);
  return UNITY_END();
}
