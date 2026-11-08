// The tour's button rules and content, against the spec: docs/superpowers/specs/2026-11-08-guided-tour-design.md
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#define PROGMEM
struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont10pt.h"
#include "DejaVu9.h"

#include "../../sketch/sketch_jan12a/guide_core.h"
#include "../../sketch/sketch_jan12a/music_songs.h"
#include "../../sketch/sketch_jan12a/sky_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static GuideState at(GuideStep step, uint8_t page = 0, bool answered = true) {
  GuideState s;
  guideStart(s);
  s.step = step;
  s.page = page;
  s.answered = answered;
  return s;
}

static GuideEvent goBack(GuideState& s) {
  guidePress(s, GUIDE_BTN_B);
  guidePress(s, GUIDE_BTN_C);
  return guidePress(s, GUIDE_BTN_A);
}

static void ask_records_on_A_and_menu_on_B() {
  GuideState s = at(GUIDE_ASK);
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_NONE);
  CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_RECORD);
  CHECK(s.step == GUIDE_ASK && !s.menuOpen);
  CHECK(guidePress(s, GUIDE_BTN_B) == GUIDE_EV_MENU);
  CHECK(s.menuOpen && s.menuSel == 0);
}

static void ask_menu_has_no_back_row() {
  GuideMenuItem rows[3];
  GuideState s = at(GUIDE_ASK);
  CHECK(guideMenuRows(s, rows) == 2);
  CHECK(rows[0] == GUIDE_MENU_GO && rows[1] == GUIDE_MENU_END);
}

static void other_menus_have_three_rows() {
  GuideMenuItem rows[3];
  GuideState s = at(GUIDE_ANSWER);
  CHECK(guideMenuRows(s, rows) == 3);
  CHECK(rows[0] == GUIDE_MENU_GO && rows[1] == GUIDE_MENU_BACK && rows[2] == GUIDE_MENU_END);
}

static void demo_A_never_acts_directly() {
  for (GuideStep step : {GUIDE_ANSWER, GUIDE_PHOTO, GUIDE_SKY, GUIDE_MUSIC, GUIDE_GRID}) {
    GuideState s = at(step);
    CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_MENU);
    CHECK(s.menuOpen && s.step == step);
  }
}

static void double_A_continues() {
  GuideState s = at(GUIDE_SKY);
  guidePress(s, GUIDE_BTN_A);
  CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_MENU);
  CHECK(s.step == GUIDE_SKY && !s.menuOpen);
}

static void menu_C_wraps_and_B_closes() {
  GuideState s = at(GUIDE_SKY);
  guidePress(s, GUIDE_BTN_A);
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.menuSel == 1);
  guidePress(s, GUIDE_BTN_C);
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.menuSel == 0);
  guidePress(s, GUIDE_BTN_C);
  CHECK(guidePress(s, GUIDE_BTN_B) == GUIDE_EV_MENU);
  CHECK(!s.menuOpen && s.step == GUIDE_SKY);
  guidePress(s, GUIDE_BTN_A);
  CHECK(s.menuSel == 0);  // Opens on "continue" every time
}

static void end_row_finishes() {
  GuideState s = at(GUIDE_SKY);
  guidePress(s, GUIDE_BTN_A);
  guidePress(s, GUIDE_BTN_C);
  guidePress(s, GUIDE_BTN_C);
  CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_FINISH);
  CHECK(s.step == GUIDE_OFF);
  GuideState ask = at(GUIDE_ASK);
  guidePress(ask, GUIDE_BTN_B);
  guidePress(ask, GUIDE_BTN_C);
  CHECK(guidePress(ask, GUIDE_BTN_A) == GUIDE_EV_FINISH);
}

static void back_table() {
  struct { GuideStep from; uint8_t page; GuideStep to; uint8_t toPage; } rows[] = {
    {GUIDE_ANSWER, 0, GUIDE_ASK, 0},   {GUIDE_RETRY, 0, GUIDE_ASK, 0},   {GUIDE_PHOTO, 0, GUIDE_ANSWER, 0},
    {GUIDE_SKY, 0, GUIDE_PHOTO, 0},    {GUIDE_MUSIC, 0, GUIDE_SKY, 0},   {GUIDE_GRID, 0, GUIDE_MUSIC, 0},
    {GUIDE_GRID, 2, GUIDE_GRID, 1},    {GUIDE_DONE, 0, GUIDE_GRID, GUIDE_PAGES - 1},
  };
  for (const auto& r : rows) {
    GuideState s = at(r.from, r.page);
    s.quote = false;
    goBack(s);
    CHECK(s.step == r.to && s.page == r.toPage && !s.quote && !s.menuOpen);
  }
}

static void back_from_photo_after_retry_goes_to_retry() {
  GuideState s = at(GUIDE_RETRY, 0, false);
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.step == GUIDE_PHOTO);
  s.quote = false;
  goBack(s);
  CHECK(s.step == GUIDE_RETRY);
}

static void grid_pages_forward_then_done() {
  GuideState s = at(GUIDE_GRID);
  for (int i = 1; i < GUIDE_PAGES; i++) {
    CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_PAGE);
    CHECK(s.page == i && s.step == GUIDE_GRID);
  }
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_DONE);
}

static void back_on_first_grid_page_goes_to_music() {
  GuideState s = at(GUIDE_GRID);
  CHECK(goBack(s) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_MUSIC);
}

static void forward_sets_quote_and_C_skips_it() {
  GuideState s = at(GUIDE_ANSWER);
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_PHOTO && s.quote);
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_QUOTE_SKIP);
  CHECK(s.step == GUIDE_PHOTO && !s.quote);
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.step == GUIDE_SKY && s.quote);
  s.quote = false;
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.step == GUIDE_MUSIC && s.quote);
  s.quote = false;
  guidePress(s, GUIDE_BTN_C);
  CHECK(s.step == GUIDE_GRID && !s.quote && s.page == 0);
}

static void retry_C_goes_to_photo() {
  GuideState s = at(GUIDE_RETRY, 0, false);
  CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_RECORD);
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_PHOTO);
}

static void done_A_finishes_and_asks() {
  GuideState s = at(GUIDE_DONE);
  CHECK(guidePress(s, GUIDE_BTN_C) == GUIDE_EV_NONE);
  CHECK(guidePress(s, GUIDE_BTN_A) == GUIDE_EV_FINISH_ASK);
  CHECK(s.step == GUIDE_OFF);
}

static void voice_results() {
  GuideState s = at(GUIDE_ASK, 0, false);
  CHECK(guideVoiceResult(s, GUIDE_VOICE_CANCELLED) == GUIDE_EV_NONE);
  CHECK(s.step == GUIDE_ASK && !s.answered);
  CHECK(guideVoiceResult(s, GUIDE_VOICE_NOT_HEARD) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_RETRY && !s.answered);
  CHECK(guideVoiceResult(s, GUIDE_VOICE_ANSWERED) == GUIDE_EV_STEP);
  CHECK(s.step == GUIDE_ANSWER && s.answered);
  GuideState b = at(GUIDE_ASK, 0, false);
  CHECK(guideVoiceResult(b, GUIDE_VOICE_BLOCKED) == GUIDE_EV_ABORT);
  CHECK(b.step == GUIDE_OFF);
  GuideState c = at(GUIDE_RETRY, 0, false);
  CHECK(guideVoiceResult(c, GUIDE_VOICE_CANCELLED) == GUIDE_EV_STEP);
  CHECK(c.step == GUIDE_ASK);
}

static void auto_start_conditions() {
  CHECK(guideShouldAutoStart(false, false, true, true));
  CHECK(!guideShouldAutoStart(true, false, true, true));
  CHECK(!guideShouldAutoStart(false, true, true, true));
  CHECK(!guideShouldAutoStart(false, false, false, true));
  CHECK(!guideShouldAutoStart(false, false, true, false));
}

static void settings_blocked_while_running() {
  CHECK(guideBlocksSettings(at(GUIDE_ASK)));
  CHECK(guideBlocksSettings(at(GUIDE_GRID)));
  CHECK(!guideBlocksSettings(at(GUIDE_OFF)));
}

static void press_only_when_awake() {
  CHECK(guideAcceptsPress(true, false));
  CHECK(!guideAcceptsPress(false, false));
  CHECK(!guideAcceptsPress(true, true));
}

static void keep_awake_expires() {
  CHECK(guideKeepAwake(299999, 0));
  CHECK(!guideKeepAwake(300000, 0));
  CHECK(guideKeepAwake(5, 0xFFFFFFF0u));
}

static void rotate_needs_full_cycle() {
  GuideRotate r = {false};
  CHECK(!guideRotateStep(r, true));
  CHECK(!guideRotateStep(r, false));
  CHECK(!guideRotateStep(r, false));
  CHECK(guideRotateStep(r, true));
  CHECK(!guideRotateStep(r, true));
}

static void slide_converges_without_overshoot() {
  float pos = 0;
  for (int i = 0; i < 200; i++) {
    pos = guideSlideStep(pos, 1, 0.04f);
    CHECK(pos <= 1.0f);
  }
  CHECK(pos == 1.0f);
  pos = 3;
  for (int i = 0; i < 200; i++) pos = guideSlideStep(pos, 2, 0.04f);
  CHECK(pos == 2.0f);
}

static int textWidth(const GFXfont& font, const std::string& text) {
  int width = 0;
  for (size_t i = 0; i < text.size();) {
    uint32_t codepoint = (unsigned char)text[i++];
    if (codepoint >= 0xC0 && i < text.size()) codepoint = ((codepoint & 0x1F) << 6) | ((unsigned char)text[i++] & 0x3F);
    if (codepoint >= font.first && codepoint <= font.last) width += font.glyph[codepoint - font.first].xAdvance;
  }
  return width;
}

static int linesNeeded(const GFXfont& font, const std::string& text, int maxWidth) {
  int lines = 1;
  std::string line, word;
  const std::string spaced = text + " ";
  for (char ch : spaced) {
    if (ch != ' ') {
      word += ch;
      continue;
    }
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (textWidth(font, candidate) > maxWidth && !line.empty()) {
      lines++;
      line = word;
    } else {
      line = candidate;
    }
    word.clear();
  }
  return lines;
}

static void tiles_are_complete_and_fit() {
  CHECK(GUIDE_PAGES * GUIDE_TILES_PER_PAGE == 16);
  for (int i = 0; i < GUIDE_PAGES * GUIDE_TILES_PER_PAGE; i++) {
    const GuideTile& t = GUIDE_TILES[i];
    for (const char* say : {t.sayDe, t.sayEn}) {
      for (const char* p = say; *p; p++) CHECK((unsigned char)*p < 0x7F);
      CHECK(!strstr(say, "Weltkrieg") && !strstr(say, "World War"));
      const int lines = linesNeeded(DejaVu9, say, 105);
      if (lines > 2) fprintf(stderr, "  too long: %s\n", say);
      CHECK(lines <= 2);
    }
    for (const char* title : {t.titleDe, t.titleEn}) {
      const int width = textWidth(Roboto_Regular10pt8b, title);
      if (width > 86) fprintf(stderr, "  too wide (%d): %s\n", width, title);
      CHECK(width <= 86);  // Tile 116 wide, title starts at +29
    }
  }
  CHECK(strcmp(GUIDE_TILES[0].titleDe, "Drucken") == 0);
  CHECK(strcmp(GUIDE_TILES[1].sayDe, "Zeitstrahl der Raumfahrt") == 0);
}

static void references_open() {
  ModelRef ref;
  CHECK(parseModelRef(GUIDE_SKY_REF, ref) && strcmp(ref.kind, "sky") == 0);
  CHECK(skyFindConstellation(ref.query) != SKY_NOT_FOUND);
  CHECK(parseModelRef(GUIDE_MUSIC_REF, ref) && strcmp(ref.kind, "music") == 0);
  CHECK(musicFindBuiltIn(ref.query) != nullptr);
}

int main() {
  ask_records_on_A_and_menu_on_B();
  ask_menu_has_no_back_row();
  other_menus_have_three_rows();
  demo_A_never_acts_directly();
  double_A_continues();
  menu_C_wraps_and_B_closes();
  end_row_finishes();
  back_table();
  back_from_photo_after_retry_goes_to_retry();
  grid_pages_forward_then_done();
  back_on_first_grid_page_goes_to_music();
  forward_sets_quote_and_C_skips_it();
  retry_C_goes_to_photo();
  done_A_finishes_and_asks();
  voice_results();
  auto_start_conditions();
  settings_blocked_while_running();
  press_only_when_awake();
  keep_awake_expires();
  rotate_needs_full_cycle();
  slide_converges_without_overshoot();
  tiles_are_complete_and_fit();
  references_open();
  printf("guide: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
