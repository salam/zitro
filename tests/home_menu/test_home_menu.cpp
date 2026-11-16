#include <algorithm>
// The home menu: its functions and example questions, against the spec: docs/superpowers/specs/2026-11-15-home-menu-design.md
#include <cstdint>
#include <cstdio>
#include <cstring>

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

#include "../../sketch/sketch_jan12a/home_menu_core.h"
#include "../../sketch/sketch_jan12a/home_menu_icons.h"
#include "../../sketch/sketch_jan12a/model_core.h"
#include "../../sketch/sketch_jan12a/anatomy_core.h"
#include "../../sketch/sketch_jan12a/clock_core.h"
#include "../../sketch/sketch_jan12a/crystal_core.h"
#include "../../sketch/sketch_jan12a/graph_core.h"
#include "../../sketch/sketch_jan12a/music_songs.h"
#include "../../sketch/sketch_jan12a/physics_core.h"
#include "../../sketch/sketch_jan12a/sky_core.h"
#include "../../sketch/sketch_jan12a/solid_core.h"
#include "../../sketch/sketch_jan12a/timeline_topics.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

// Width in the menu font (Roboto 10 pt, Latin-1 glyphs from 0x20): the UTF-8 text decoded, xAdvance summed.
// A character the font doesn't have counts as far too wide.
static int menuWidth(const char* s) {
  int w = 0;
  const unsigned char* p = (const unsigned char*)s;
  while (*p) {
    uint32_t cp;
    if (*p < 0x80) cp = *p++;
    else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
    else return 9999;
    if (cp < 0x20 || cp > 0xFF) return 9999;
    w += Roboto_Regular10pt8bGlyphs[cp - 0x20].xAdvance;
  }
  return w;
}

static const int HOME_LABEL_MAX_PX = 174;  // x 50 to the end of the selected row at x 224

static void test_count() { CHECK(HOME_QUESTION_COUNT == 16); }

static void test_refs_parse() {
  int photos = 0;
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) {
    const HomeQuestion& q = HOME_QUESTIONS[i];
    CHECK(q.imageQuery && q.imageQuery[0]);  // also the fallback picture when a view can't open
    if (!q.model[0]) { photos++; continue; }
    ModelRef ref;
    if (!parseModelRef(q.model, ref)) { fprintf(stderr, "  bad ref %s\n", q.model); CHECK(false); }
  }
  CHECK(photos == 1);
}

// Every reference by its feature's own finder, on the query the table's ref yields
static void test_ids_known() {
  int found = 0;
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) {
    ModelRef ref;
    if (!parseModelRef(HOME_QUESTIONS[i].model, ref)) continue;
    const char* k = ref.kind;
    const char* q = ref.query;
    bool ok = true;
    if (!strcmp(k, "sky")) ok = skyFindConstellation(q) >= 0;
    else if (!strcmp(k, "anatomy")) { int idx; bool cut; ok = anatomyParseQuery(q, idx, cut) && !cut; }
    else if (!strcmp(k, "crystal")) ok = crystalResolve(q) >= 0;
    else if (!strcmp(k, "solid")) ok = solidFindId(q) >= 0;
    else if (!strcmp(k, "physics")) { int s, e; ok = physicsParseQuery(q, s, e); }
    else if (!strcmp(k, "clock")) ok = clockFindCity(q) >= 0;
    else if (!strcmp(k, "music")) ok = musicFindBuiltIn(q) != nullptr;
    else if (!strcmp(k, "timeline")) ok = strstr(TIMELINE_TOPIC_LIST, q) != nullptr;
    else if (!strcmp(k, "graph")) { static GraphProgram g; ok = graphCompile(q, g); }
    else if (!strcmp(k, "iss")) ok = !strcmp(q, "now");
    else continue;  // dino, protein, molecule, globe, recipe: looked up online, checked on the stick
    found++;
    if (!ok) { fprintf(stderr, "  unknown id %s\n", HOME_QUESTIONS[i].model); CHECK(false); }
  }
  CHECK(found == 10);
}

// searchWikipedia asks en.wikipedia.org's summary for this exact title, so it must be an English title (plain
// ASCII). Each was checked live on 2026-11-15 to have a picture, except the song (built in, never needs one).
static void test_image_queries_english() {
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) {
    bool ascii = true;
    for (const char* p = HOME_QUESTIONS[i].imageQuery; *p; p++) ascii = ascii && (unsigned char)*p < 0x80;
    if (!ascii) fprintf(stderr, "  not an English title: %s\n", HOME_QUESTIONS[i].imageQuery);
    CHECK(ascii);
  }
  CHECK(!strcmp(HOME_QUESTIONS[0].imageQuery, "Orion (constellation)"));
  CHECK(!strcmp(HOME_QUESTIONS[4].imageQuery, "Marie Curie"));
}

static void test_no_cost_kinds() {
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) CHECK(strncmp(HOME_QUESTIONS[i].model, "wolfram:", 8) != 0);
}

static void test_labels_fit() {
  CHECK(menuWidth("Foto von Marie Curie") > HOME_LABEL_MAX_PX);  // the measure is real: 185 px
  CHECK(menuWidth("WLAN zurücksetzen") == 180);
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) {
    const HomeQuestion& q = HOME_QUESTIONS[i];
    const int de = menuWidth(q.labelDe), en = menuWidth(q.labelEn);
    if (de > HOME_LABEL_MAX_PX || en > HOME_LABEL_MAX_PX) fprintf(stderr, "  too wide: %s (%d) / %s (%d)\n", q.labelDe, de, q.labelEn, en);
    CHECK(de <= HOME_LABEL_MAX_PX);
    CHECK(en <= HOME_LABEL_MAX_PX);
  }
}

static void test_answers_present() {
  for (int i = 0; i < HOME_QUESTION_COUNT; i++) {
    const HomeQuestion& q = HOME_QUESTIONS[i];
    CHECK(q.answerDe[0] && strlen(q.answerDe) < 80);
    CHECK(q.answerEn[0] && strlen(q.answerEn) < 80);
  }
}

static void test_function_labels_fit() {
  CHECK(HOME_FN_COUNT == 6);
  for (int f = 0; f < HOME_FN_COUNT; f++) {
    for (int lang = 0; lang < 2; lang++) {
      const int w = menuWidth(HOME_FUNCTION_LABELS[f][lang]);
      if (w > HOME_LABEL_MAX_PX) fprintf(stderr, "  too wide: %s (%d)\n", HOME_FUNCTION_LABELS[f][lang], w);
      CHECK(HOME_FUNCTION_LABELS[f][lang][0] && w <= HOME_LABEL_MAX_PX);
    }
  }
  CHECK(!strcmp(HOME_FUNCTION_LABELS[HOME_FN_TIMERS][0], "Timer anzeigen"));  // the user's prominent row, on top
  CHECK(HOME_FN_TIMERS == 0);
}

static void test_rows() {
  CHECK(HOME_ROW_COUNT == 22);
  CHECK(homeRowQuestion(0) == -1);
  CHECK(homeRowQuestion(5) == -1);
  CHECK(homeRowQuestion(6) == 0);
  CHECK(homeRowQuestion(21) == 15);
  CHECK(homeRowQuestion(22) == -1);
  CHECK(homeRowQuestion(-1) == -1);
}

static void test_presses() {
  uint8_t sel = 0;
  CHECK(homeMenuPress(sel, HOME_PRESS_NEXT) == HOME_ACT_REDRAW && sel == 1);
  sel = 21;
  CHECK(homeMenuPress(sel, HOME_PRESS_NEXT) == HOME_ACT_REDRAW && sel == 0);
  sel = 7;
  CHECK(homeMenuPress(sel, HOME_PRESS_BACK) == HOME_ACT_CLOSE && sel == 7);
  CHECK(homeMenuPress(sel, HOME_PRESS_A) == HOME_ACT_CHOOSE && sel == 7);
  CHECK(homeFirstVisible(0, 22) == 0);
  CHECK(homeFirstVisible(1, 22) == 0);
  CHECK(homeFirstVisible(5, 22) == 4);
  CHECK(homeFirstVisible(21, 22) == 19);
}

static void test_night_lock() {
  CHECK(!homeMenuBlockedAtNight(HOME_FN_TIMERS));
  CHECK(!homeMenuBlockedAtNight(HOME_FN_SETTINGS));
  CHECK(!homeMenuBlockedAtNight(HOME_FN_CLOSE));
  CHECK(homeMenuBlockedAtNight(HOME_FN_TOUR));
  CHECK(homeMenuBlockedAtNight(HOME_FN_SURPRISE));
  CHECK(homeMenuBlockedAtNight(HOME_FN_WORDS));
  CHECK(homeMenuBlockedAtNight(6));
  CHECK(homeMenuBlockedAtNight(21));
}

// Matthias, 2026-11-16: in a menu a double click of B has no job of its own, C isn't read, so each click of B is the next
// row at once; holding B goes back. The home menu and the trainer's lists read their presses through this.
static void test_menu_press_read() {
  CHECK(menuPressRead(false, false, false) == MENU_PRESS_NONE);
  CHECK(menuPressRead(true, false, false) == MENU_PRESS_A);
  CHECK(menuPressRead(true, true, false) == MENU_PRESS_A);
  CHECK(menuPressRead(false, true, false) == MENU_PRESS_NEXT);
  CHECK(menuPressRead(false, false, true) == MENU_PRESS_BACK);
  CHECK(menuPressRead(false, true, true) == MENU_PRESS_BACK);
  // Two quick clicks are two rows down, never a press of their own
  uint8_t sel = HOME_FN_TIMERS;
  for (int k = 0; k < 2; k++) {
    const MenuPress p = menuPressRead(false, true, false);
    CHECK(p == MENU_PRESS_NEXT);
    CHECK(homeMenuPress(sel, homePressFromMenu(p)) == HOME_ACT_REDRAW);
  }
  CHECK(sel == HOME_FN_SURPRISE);
  CHECK(homePressFromMenu(MENU_PRESS_A) == HOME_PRESS_A);
  CHECK(homePressFromMenu(MENU_PRESS_BACK) == HOME_PRESS_BACK);
}

// Matthias, 2026-11-16: icons instead of the ▶ — gears for the settings, a timer, the tour, a parcel for the surprise,
// and (stretch) one for each question. Every row has one, with ink in it, and it is an outline, not a filled block.
static void test_row_icons() {
  CHECK(sizeof(HOME_ROW_ICONS) / sizeof(HOME_ROW_ICONS[0]) == (size_t)HOME_ROW_COUNT);
  CHECK(sizeof(HOME_ROW_ICON_NAMES) / sizeof(HOME_ROW_ICON_NAMES[0]) == (size_t)HOME_ROW_COUNT);
  for (int row = 0; row < HOME_ROW_COUNT; row++) {
    int most = 0;
    long sum = 0;
    for (int k = 0; k < HOME_ICON_SIZE * HOME_ICON_SIZE; k++) {
      most = std::max(most, (int)HOME_ROW_ICONS[row][k]);
      sum += HOME_ROW_ICONS[row][k];
    }
    if (most < 200 || sum / (HOME_ICON_SIZE * HOME_ICON_SIZE) > 160) {
      fprintf(stderr, "  icon of row %d (%s) is empty or a block\n", row + 1, HOME_ROW_ICON_NAMES[row]);
      CHECK(false);
    }
  }
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_TIMERS], "timer"));
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_TOUR], "tour"));
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_SURPRISE], "featured_seasonal_and_gifts"));
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_SETTINGS], "settings"));
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_CLOSE], "close"));
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_COUNT + 0], "star"));   // Wo ist Orion?
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_COUNT + 5], "trex"));   // T-Rex
  CHECK(!strcmp(HOME_ROW_ICON_NAMES[HOME_FN_COUNT + 15], "music_note"));  // Twinkle Twinkle
  // Drawn at x 26..45, so the label at x 50 keeps its place and its 174 px
  CHECK(HOME_ICON_X + HOME_ICON_SIZE <= 50 - 4);
  CHECK(HOME_ICON_X >= 20 + 4);  // inside the selected row's white box (x 20)
}

int main() {
  test_count();
  test_refs_parse();
  test_ids_known();
  test_image_queries_english();
  test_no_cost_kinds();
  test_labels_fit();
  test_answers_present();
  test_function_labels_fit();
  test_rows();
  test_presses();
  test_night_lock();
  test_menu_press_read();
  test_row_icons();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
