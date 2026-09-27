#include <unity.h>

#include "lb/Color.h"

using namespace lb;

void setUp() {}
void tearDown() {}

static void assertColor(Rgb expected, Rgb actual) {
  TEST_ASSERT_EQUAL_UINT8(expected.r, actual.r);
  TEST_ASSERT_EQUAL_UINT8(expected.g, actual.g);
  TEST_ASSERT_EQUAL_UINT8(expected.b, actual.b);
}

void test_named_colors() {
  Rgb c;
  TEST_ASSERT_TRUE(parseColor("red", c));
  assertColor(Rgb(255, 0, 0), c);
  TEST_ASSERT_TRUE(parseColor("Orange", c));
  assertColor(Rgb(255, 165, 0), c);
  TEST_ASSERT_TRUE(parseColor("warm white", c));
  Rgb same;
  TEST_ASSERT_TRUE(parseColor("Warm_White", same));
  assertColor(c, same);
  TEST_ASSERT_TRUE(parseColor("  sky-blue ", c));
  assertColor(Rgb(80, 190, 255), c);
}

void test_hex_colors() {
  Rgb c;
  TEST_ASSERT_TRUE(parseColor("#FF8800", c));
  assertColor(Rgb(255, 136, 0), c);
  TEST_ASSERT_TRUE(parseColor("#f80", c));
  assertColor(Rgb(255, 136, 0), c);
  TEST_ASSERT_TRUE(parseColor("00ff7f", c));
  assertColor(Rgb(0, 255, 127), c);
}

void test_rejects_non_colors() {
  Rgb c(1, 2, 3);
  TEST_ASSERT_FALSE(parseColor("", c));
  TEST_ASSERT_FALSE(parseColor("nope", c));
  TEST_ASSERT_FALSE(parseColor("#12345", c));
  TEST_ASSERT_FALSE(parseColor("#ggg", c));
  TEST_ASSERT_FALSE(parseColor("a-very-long-name-that-is-not-a-color", c));
  TEST_ASSERT_FALSE(parseColor("rainbow", c));
  assertColor(Rgb(1, 2, 3), c);  // untouched on failure
}

void test_rainbow_is_a_mode() {
  TEST_ASSERT_TRUE(isRainbow("rainbow"));
  TEST_ASSERT_TRUE(isRainbow(" Rainbow "));
  TEST_ASSERT_FALSE(isRainbow("red"));
}

void test_hsv_primaries() {
  assertColor(Rgb(255, 0, 0), hsv(0, 255, 255));
  assertColor(Rgb(0, 255, 0), hsv(120, 255, 255));
  assertColor(Rgb(0, 0, 255), hsv(240, 255, 255));
  assertColor(hsv(0, 255, 255), hsv(360, 255, 255));
  assertColor(hsv(300, 255, 255), hsv(-60, 255, 255));
  assertColor(Rgb(255, 255, 255), hsv(42, 0, 255));
}

void test_scale() {
  assertColor(Rgb(128, 64, 0), scale(Rgb(255, 128, 0), 128));
  assertColor(Rgb(255, 128, 0), scale(Rgb(255, 128, 0), 255));
  assertColor(Rgb(0, 0, 0), scale(Rgb(255, 128, 0), 0));
}

void test_color_name_list_parses() {
  for (size_t i = 0; kColorNames[i] != nullptr; ++i) {
    Rgb c;
    const bool ok = parseColor(kColorNames[i], c) || isRainbow(kColorNames[i]);
    TEST_ASSERT_TRUE_MESSAGE(ok, kColorNames[i]);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_named_colors);
  RUN_TEST(test_hex_colors);
  RUN_TEST(test_rejects_non_colors);
  RUN_TEST(test_rainbow_is_a_mode);
  RUN_TEST(test_hsv_primaries);
  RUN_TEST(test_scale);
  RUN_TEST(test_color_name_list_parses);
  return UNITY_END();
}
