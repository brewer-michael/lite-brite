#include <unity.h>

#include <string>

#include "lb/Font.h"
#include "lb/Icons.h"
#include "lb/Text.h"

using namespace lb;

void setUp() {}
void tearDown() {}

static uint32_t decodeAll(const std::string& s, size_t& i) { return decodeUtf8(s, i); }

static uint32_t glyphCodepoint(const TextItem& item) { return font_data::kGlyphs[item.ref].codepoint; }

static std::string glyphString(const TextLine& line) {
  std::string out;
  for (const auto& item : line.items()) {
    if (item.kind == ItemKind::Icon) {
      out += std::string("[") + iconInfo(item.ref).name + "]";
    } else {
      const uint32_t cp = glyphCodepoint(item);
      if (cp < 0x80) {
        out += static_cast<char>(cp);
      } else {
        out += "?";
      }
    }
  }
  return out;
}

void test_utf8_decoding() {
  size_t i = 0;
  const std::string s = "A\xC3\xA9\xE2\x80\x99\xF0\x9F\x94\x91";  // A é ’ 🔑
  TEST_ASSERT_EQUAL_UINT32('A', decodeAll(s, i));
  TEST_ASSERT_EQUAL_UINT32(0xE9, decodeAll(s, i));
  TEST_ASSERT_EQUAL_UINT32(0x2019, decodeAll(s, i));
  TEST_ASSERT_EQUAL_UINT32(0x1F511, decodeAll(s, i));
  TEST_ASSERT_EQUAL(s.size(), i);
}

void test_utf8_malformed_input_is_replaced() {
  size_t i = 0;
  const std::string bad = "\xFF" "a" "\xE2\x80";  // invalid lead byte, then a truncated sequence
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(bad, i));
  TEST_ASSERT_EQUAL_UINT32('a', decodeAll(bad, i));
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(bad, i));
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(bad, i));
  TEST_ASSERT_EQUAL(bad.size(), i);
  i = 0;
  const std::string overlong = "\xC0\xAF";  // overlong '/'
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, decodeAll(overlong, i));
}

void test_width_is_glyphs_plus_spacing() {
  TextLine line;
  line.set("Hi");
  TEST_ASSERT_EQUAL(2, line.items().size());
  TEST_ASSERT_EQUAL(5 + 1 + 1, line.width());
  TEST_ASSERT_EQUAL(0, line.items()[0].x);
  TEST_ASSERT_EQUAL(6, line.items()[1].x);
}

void test_keys_width() {
  TextLine line;
  line.set("Keys");
  TEST_ASSERT_EQUAL(5 + 1 + 4 + 1 + 4 + 1 + 4, line.width());
}

void test_whitespace_collapses_and_trims() {
  TextLine line;
  line.set("  a \t\n  b  ");
  TEST_ASSERT_EQUAL_STRING("a b", glyphString(line).c_str());
}

void test_empty_text() {
  TextLine line;
  line.set("   ");
  TEST_ASSERT_TRUE(line.empty());
  TEST_ASSERT_EQUAL(0, line.width());
}

void test_icon_markup() {
  TextLine line;
  line.set(":key: Keys");
  TEST_ASSERT_EQUAL_STRING("[key] Keys", glyphString(line).c_str());
  TEST_ASSERT_EQUAL(ItemKind::Icon, line.items()[0].kind);
  TEST_ASSERT_EQUAL(8, line.items()[0].width);
}

void test_icon_markup_is_case_insensitive_and_uses_aliases() {
  TextLine line;
  line.set(":Home::love:");
  TEST_ASSERT_EQUAL_STRING("[house][heart]", glyphString(line).c_str());
}

void test_unknown_icon_and_times_stay_literal() {
  TextLine line;
  line.set(":nope: at 5:30");
  TEST_ASSERT_EQUAL_STRING(":nope: at 5:30", glyphString(line).c_str());
}

void test_emoji_become_icons() {
  TextLine line;
  line.set("Hi \xF0\x9F\x94\x91 \xE2\x9D\xA4\xEF\xB8\x8F");  // Hi 🔑 ❤️ (with variation selector)
  TEST_ASSERT_EQUAL_STRING("Hi [key] [heart]", glyphString(line).c_str());
}

void test_unknown_emoji_are_dropped() {
  TextLine line;
  line.set("a\xF0\x9F\xA6\x84" "b");  // a🦄b
  TEST_ASSERT_EQUAL_STRING("ab", glyphString(line).c_str());
}

void test_color_markup() {
  TextLine line;
  line.set("{red}A{}B{ff8800}C");
  TEST_ASSERT_EQUAL(3, line.items().size());
  TEST_ASSERT_TRUE(line.items()[0].colored);
  TEST_ASSERT_TRUE(line.items()[0].color == Rgb(255, 0, 0));
  TEST_ASSERT_FALSE(line.items()[1].colored);
  TEST_ASSERT_TRUE(line.items()[2].colored);
  TEST_ASSERT_TRUE(line.items()[2].color == Rgb(255, 0x88, 0));
}

void test_braces_that_are_not_markup() {
  TextLine line;
  line.set("{{x} {nope} {");
  TEST_ASSERT_EQUAL_STRING("{x} {nope} {", glyphString(line).c_str());
}

void test_smart_punctuation_and_accents_fold() {
  TextLine line;
  line.set("Don\xE2\x80\x99t \xE2\x80\x9C" "Caf\xC3\xA9\xE2\x80\x9D \xE2\x80\x94 \xC3\x9F");  // Don’t “Café” — ß
  TEST_ASSERT_EQUAL_STRING("Don't \"Cafe\" - ss", glyphString(line).c_str());
}

void test_unsupported_characters_show_a_box() {
  TextLine line;
  line.set("\xE5\xAD\x97");  // 字
  TEST_ASSERT_EQUAL(1, line.items().size());
  TEST_ASSERT_EQUAL_UINT32(kReplacementChar, glyphCodepoint(line.items()[0]));
}

void test_degree_sign_has_a_glyph() {
  TextLine line;
  line.set("72\xC2\xB0");  // 72°
  TEST_ASSERT_EQUAL(3, line.items().size());
  TEST_ASSERT_EQUAL_UINT32(0xB0, glyphCodepoint(line.items()[2]));
}

void test_every_printable_ascii_has_a_glyph() {
  for (uint32_t cp = 0x20; cp < 0x7F; ++cp) TEST_ASSERT_NOT_NULL(findGlyph(cp));
  TEST_ASSERT_NULL(findGlyph(0x7F));
}

void test_icon_lookup() {
  TEST_ASSERT_TRUE(findIconByName("key") >= 0);
  TEST_ASSERT_EQUAL(findIconByName("key"), findIconByName("keys"));
  TEST_ASSERT_EQUAL(-1, findIconByName("definitely_not_an_icon"));
  TEST_ASSERT_EQUAL(findIconByName("clock"), findIconByCodepoint(0x1F555));  // clock-face range
  TEST_ASSERT_EQUAL(-1, findIconByCodepoint('A'));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_utf8_decoding);
  RUN_TEST(test_utf8_malformed_input_is_replaced);
  RUN_TEST(test_width_is_glyphs_plus_spacing);
  RUN_TEST(test_keys_width);
  RUN_TEST(test_whitespace_collapses_and_trims);
  RUN_TEST(test_empty_text);
  RUN_TEST(test_icon_markup);
  RUN_TEST(test_icon_markup_is_case_insensitive_and_uses_aliases);
  RUN_TEST(test_unknown_icon_and_times_stay_literal);
  RUN_TEST(test_emoji_become_icons);
  RUN_TEST(test_unknown_emoji_are_dropped);
  RUN_TEST(test_color_markup);
  RUN_TEST(test_braces_that_are_not_markup);
  RUN_TEST(test_smart_punctuation_and_accents_fold);
  RUN_TEST(test_unsupported_characters_show_a_box);
  RUN_TEST(test_degree_sign_has_a_glyph);
  RUN_TEST(test_every_printable_ascii_has_a_glyph);
  RUN_TEST(test_icon_lookup);
  return UNITY_END();
}
