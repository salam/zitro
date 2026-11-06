// Host tests for the recipe view's pure logic. Build and run: tests/recipe/run.sh
#include <dirent.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/recipe_core.h"

// The stick's real font, so "fits the screen" is checked with the real letter widths
#define PROGMEM
struct GFXglyph {
  uint32_t bitmapOffset;
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

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static const int STEP_WIDTH = RECIPE_STEP_WIDTH;
static const int INGREDIENT_WIDTH = RECIPE_INGREDIENT_WIDTH;

static RecipeData recipe;
static RecipeLayout layout;
static char scratch[RECIPE_POOL_SIZE];

static std::string readFile(const std::string& path) {
  std::string out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { fprintf(stderr, "missing file %s\n", path.c_str()); exit(2); }
  char buf[4096]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  fclose(f);
  return out;
}

static bool decode(const char*& p, const char* end, uint32_t& code) {
  const unsigned char c = (unsigned char)*p;
  size_t n = recipeCharBytes(p);
  if ((size_t)(end - p) < n) n = end - p;
  if (n == 1) code = c;
  else if (n == 2) code = ((c & 0x1F) << 6) | ((unsigned char)p[1] & 0x3F);
  else if (n == 3) code = ((c & 0x0F) << 12) | (((unsigned char)p[1] & 0x3F) << 6) | ((unsigned char)p[2] & 0x3F);
  else code = 0xFFFF;
  p += n;
  return true;
}

static int fontWidth(const char* text, size_t length, void*) {
  int width = 0;
  const char* p = text;
  const char* end = text + length;
  while (p < end) {
    uint32_t code;
    decode(p, end, code);
    if (code >= Roboto_Regular10pt8b.first && code <= Roboto_Regular10pt8b.last) {
      width += Roboto_Regular10pt8bGlyphs[code - Roboto_Regular10pt8b.first].xAdvance;
    } else {
      width += 9;
    }
  }
  return width;
}

static int narrowWidth(const char*, size_t length, void*) { return (int)length * 50; }

static bool parse(const std::string& s) { return recipeParse(s.c_str(), s.size(), recipe, scratch); }
static bool adult(const char* text) {
  recipeLowerInto(text, scratch, sizeof(scratch));
  return recipeTextNeedsAdult(scratch);
}
static bool alcohol(const char* text) {
  recipeLowerInto(text, scratch, sizeof(scratch));
  return recipeTextHasAlcohol(scratch);
}

static void testQuery() {
  char out[64];
  CHECK(recipeNormalizeQuery("Spaghetti Bolognese", out, sizeof out) && !strcmp(out, "spaghetti bolognese"));
  CHECK(recipeNormalizeQuery("  PANCAKES!  ", out, sizeof out) && !strcmp(out, "pancakes"));
  CHECK(recipeNormalizeQuery("mac-and-cheese", out, sizeof out) && !strcmp(out, "mac and cheese"));
  CHECK(recipeNormalizeQuery("crêpes", out, sizeof out) && !strcmp(out, "crêpes"));
  CHECK(!recipeNormalizeQuery("", out, sizeof out));
  CHECK(!recipeNormalizeQuery(" !? ", out, sizeof out));
  CHECK(!recipeNormalizeQuery(nullptr, out, sizeof out));
  char tiny[4];
  CHECK(!recipeNormalizeQuery("pancakes", tiny, sizeof tiny));
}

static void testIndex() {
  const std::string index =
      "pancakes|pancakes|de en\n"
      "chicken soup|chicken-soup|de en\n"
      "lemonade|lemonade|en\n"
      "no slug here\n"
      "evil|../../etc/passwd|en\n"
      "evil2|UPPER|en\n"
      "nolang|nolang|\n"
      "\n"
      "last|last-one|de";
  char slug[RECIPE_SLUG_SIZE], langs[RECIPE_LANGS_SIZE];
  auto find = [&](const char* name) {
    return recipeIndexLookup(index.data(), index.size(), name, slug, sizeof slug, langs, sizeof langs);
  };
  CHECK(find("pancakes") && !strcmp(slug, "pancakes") && !strcmp(langs, "de en"));
  CHECK(find("chicken soup") && !strcmp(slug, "chicken-soup"));
  CHECK(find("last") && !strcmp(slug, "last-one") && !strcmp(langs, "de"));  // No line end at the end of the file
  CHECK(!find("soup"));         // Exact only: a near match could be another dish
  CHECK(!find("chicken"));
  CHECK(!find("no slug here"));
  CHECK(!find("evil"));
  CHECK(!find("evil2"));
  CHECK(!find("nolang"));
  CHECK(!find(""));
  CHECK(!recipeIndexLookup(nullptr, 0, "pancakes", slug, sizeof slug, langs, sizeof langs));
  // A long line and a long slug don't overflow anything
  const std::string longName = std::string(500, 'a') + "|x|en\npancakes|" + std::string(500, 'b') + "|en\n";
  CHECK(!recipeIndexLookup(longName.data(), longName.size(), "pancakes", slug, sizeof slug, langs, sizeof langs));
  // Cut off in the middle, at every length: no crash, and a broken line is never a match
  for (size_t cut = 0; cut < 24; cut++) {
    const bool found = recipeIndexLookup(index.data(), cut, "pancakes", slug, sizeof slug, langs, sizeof langs);
    CHECK(found == (cut >= strlen("pancakes|pancakes|d")));
  }
  CHECK(!strcmp(recipeChooseLang("de en", "DE"), "de"));
  CHECK(!strcmp(recipeChooseLang("de en", "EN"), "en"));
  CHECK(!strcmp(recipeChooseLang("en", "DE"), "en"));
  CHECK(!strcmp(recipeChooseLang("de", "EN"), "de"));
  CHECK(!strcmp(recipeChooseLang("de en", nullptr), "de"));
  CHECK(recipeChooseLang("fr", "DE") == nullptr);
  CHECK(recipeChooseLang("", "EN") == nullptr);
}

static void testAlcohol() {
  for (const char* text : {"1 cup red wine", "2 EL Rum", "Rotwein", "150 ml Weißwein", "Eierlikör", "Beer batter",
                           "a shot of Brandy", "1 EL WEIN", "etwas Sherry.", "(vodka)", "Bier, kalt", "Kirschwasser",
                           "Orangenlikör", "wine vinegar", "Weinessig", "Schnaps", "gin", "Rumaroma", "Sekt"}) {
    CHECK(alcohol(text));
  }
  for (const char* text : {"Schweinefleisch", "herum", "darum", "Weinsteinbackpulver", "Rumford baking powder",
                           "fresh ginger", "Krume", "whisk the eggs", "Ingwer", "begin", "kale", "Insekten", "Steinpilz",
                           "Kuchenblech"}) {
    CHECK(!alcohol(text));
  }
}

static void testAdult() {
  for (const char* text : {"Preheat the oven to 180 C.", "Bake for 20 minutes", "Fry on both sides", "Bring to a boil",
                           "Cut the apple", "Chop the onions", "in a hot pan", "Heat the milk", "Melt the butter",
                           "Simmer gently.", "Slice the bread", "Peel the potatoes", "Use a sharp knife",
                           "Im Backofen backen", "In der Pfanne braten", "Wasser kochen", "Zwiebeln schneiden",
                           "Kartoffeln schälen", "in heißem Fett", "SCHÄLEN", "fein hacken", "Butter schmelzen",
                           "mit dem Messer teilen", "in den Ofen schieben", "Topf nehmen", "Käse reiben", "auf dem Herd",
                           "Cutting the bread", "Add the slices", "It chops well", "Keep peeling", "Grating cheese",
                           "It heats up", "Let it simmered", "Steaming rice", "Butter melts", "Roasts well",
                           "Searing meat", "Preheating", "Sautéed onions", "Sauteed onions", "the broiler", "Grills well",
                           "Toasting", "use scissors", "a food processor", "Blend until smooth", "Warming oil",
                           "bei schwacher Hitze", "auf der Herdplatte", "mit der Schere"}) {
    CHECK(adult(text));
  }
  for (const char* text : {"Mix the flour and baking powder.", "Add baking soda", "Stir the milk", "Eat the pancake",
                           "Mash the potato", "Add wheat flour", "a shot glass", "Serve the biscuit", "Woven basket",
                           "Mehl und Backpulver mischen", "Eine Messerspitze Salz unterrühren", "Das heißt: gut rühren",
                           "Milch dazugeben", "Alles verrühren", "Kalt stellen"}) {
    CHECK(!adult(text));
  }
}

static const char* const GOOD =
    "{\"title\":\"Pancakes\",\"lang\":\"en\",\"source\":\"Wikibooks\",\"url\":\"https://en.wikibooks.org/wiki/x\","
    "\"revid\":1,\"license\":\"CC BY-SA 4.0\",\"ingredients\":[\"120 g flour\",\"1 egg\"],"
    "\"steps\":[\"Mix the flour and the egg.\",\"Fry in a hot pan.\"]}";

static std::string withSteps(int count, const std::string& text) {
  std::string out = "{\"title\":\"T\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[";
  for (int i = 0; i < count; i++) out += (i ? ",\"" : "\"") + text + "\"";
  return out + "]}";
}

static void testParse() {
  CHECK(parse(GOOD));
  CHECK(!strcmp(recipe.title, "Pancakes"));
  CHECK(!strcmp(recipe.lang, "en"));
  CHECK(recipe.ingredientCount == 2 && recipe.stepCount == 2);
  CHECK(!strcmp(recipe.pool + recipe.ingredient[0], "120 g flour"));
  CHECK(!strcmp(recipe.pool + recipe.step[1], "Fry in a hot pan."));

  // Cut off at every length: never a crash and never half a recipe
  const std::string good = GOOD;
  for (size_t cut = 0; cut < good.size(); cut++) CHECK(!parse(good.substr(0, cut)));

  const char* const bad[] = {
      "",
      "garbage",
      "<html>404 not found</html>",
      "null",
      "[]",
      "{}",
      "[{\"title\":\"x\"}]",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"steps\":[\"a\"]}",
      "{\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}",
      "{\"title\":\"\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"fr\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"   \"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[1,2]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[null],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":\"a\",\"steps\":[\"a\"]}",
      "{\"title\":1,\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":{\"0\":\"a\"}}",
      "{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[[[[[[[[[[[[[[[[\"a\"]]]]]]]]]]]]]]]],\"steps\":[\"a\"]}",
  };
  for (const char* s : bad) CHECK(!parse(s));

  CHECK(!recipeParse(nullptr, 10, recipe, scratch));
  CHECK(!recipeParse(GOOD, 0, recipe, scratch));

  // Alcohol anywhere refuses the file
  CHECK(!parse("{\"title\":\"Rum cake\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"b\"]}"));
  CHECK(!parse("{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"1 cup red wine\"],\"steps\":[\"b\"]}"));
  CHECK(!parse("{\"title\":\"x\",\"lang\":\"de\",\"ingredients\":[\"a\"],\"steps\":[\"Mit Rotwein ablöschen\"]}"));
  CHECK(parse("{\"title\":\"Schweinebraten\",\"lang\":\"de\",\"ingredients\":[\"Schwein\"],\"steps\":[\"herum\"]}"));

  // Limits: refused, never cut, because a missing step is worse than no recipe
  CHECK(parse(withSteps(RECIPE_MAX_STEPS, "x")));
  CHECK(recipe.stepCount == RECIPE_MAX_STEPS);
  CHECK(!parse(withSteps(RECIPE_MAX_STEPS + 1, "x")));
  CHECK(!parse(withSteps(1, std::string(RECIPE_POOL_SIZE + 10, 'x'))));
  CHECK(parse(withSteps(1, std::string(RECIPE_POOL_SIZE - 10, 'x'))));
  CHECK(!parse(withSteps(40, std::string(400, 'x'))));  // Under the body limit, over the pool: 16 KB fits, 12 KB doesn't
  CHECK(!parse(std::string(RECIPE_BODY_LIMIT + 1, ' ') + GOOD));
  CHECK(!parse("{\"title\":\"" + std::string(RECIPE_TITLE_SIZE, 'x') +
               "\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"a\"]}"));

  // Odd but valid text: control characters become spaces, broken UTF-8 doesn't crash anything later on
  CHECK(parse("{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\\tb\"],\"steps\":[\"line\\none\"]}"));
  CHECK(!strcmp(recipe.pool + recipe.ingredient[0], "a b"));
  CHECK(!strcmp(recipe.pool + recipe.step[0], "line one"));
  CHECK(parse("{\"title\":\"x\",\"lang\":\"en\",\"ingredients\":[\"a\"],\"steps\":[\"bad \xC3\"]}"));
  CHECK(recipeBuildLayout(recipe, INGREDIENT_WIDTH, STEP_WIDTH, fontWidth, nullptr, layout, scratch));
}

// Every line fits, and joined back together the lines are the text: nothing lost, every number still there
static void checkWrap(const char* text, int width, RecipeMeasure measure) {
  static RecipeLayout l;
  l.lineCount = 0;
  l.pageCount = 0;
  static char pool[RECIPE_POOL_SIZE];
  strcpy(pool, text);
  const int added = recipeWrap(pool, 0, width, measure, nullptr, l);
  CHECK(added > 0);
  std::string joined;
  for (int i = 0; i < added; i++) {
    const RecipeLine& line = l.lines[i];
    CHECK(line.length > 0);
    CHECK(line.itemStart == (i == 0));
    if (measure != narrowWidth) CHECK(measure(pool + line.start, line.length, nullptr) <= width);
    CHECK(((unsigned char)pool[line.start] & 0xC0) != 0x80);  // Never starts inside a character
    joined.append(pool + line.start, line.length);
  }
  std::string source;
  for (const char* p = text; *p; p++) if (*p != ' ') source += *p;
  std::string flat;
  for (char c : joined) if (c != ' ') flat += c;
  CHECK(flat == source);
}

static void testWrap() {
  checkWrap("Put 120 g flour, 1 1/2 tsp baking powder and 250 ml milk into a bowl and bake at 180 °C for 25 minutes.",
            STEP_WIDTH, fontWidth);
  checkWrap("Die Äpfel schälen, in Würfel schneiden und über die Rührschüssel geben.", STEP_WIDTH, fontWidth);
  checkWrap(std::string(300, 'x').c_str(), STEP_WIDTH, fontWidth);  // One word wider than the screen
  checkWrap("ÄÖÜäöüßÄÖÜäöüßÄÖÜäöüßÄÖÜäöüßÄÖÜäöüßÄÖÜäöüßÄÖÜäöüßÄÖÜäöüß", STEP_WIDTH, fontWidth);
  checkWrap("short", STEP_WIDTH, fontWidth);
  checkWrap("many    spaces   between  words", STEP_WIDTH, fontWidth);
  checkWrap("narrow screen", 10, narrowWidth);  // No character fits: still ends, one per line
  checkWrap("bad \xC3", STEP_WIDTH, fontWidth);
}

static void testLayout() {
  CHECK(parse(GOOD));
  CHECK(recipeBuildLayout(recipe, INGREDIENT_WIDTH, STEP_WIDTH, fontWidth, nullptr, layout, scratch));
  CHECK(layout.pageCount == 3);
  CHECK(layout.pages[0].step == -1 && !layout.pages[0].adult);
  CHECK(layout.lines[layout.pages[0].firstLine].itemStart);
  CHECK(layout.pages[1].step == 0 && !layout.pages[1].adult);
  CHECK(layout.pages[2].step == 1 && layout.pages[2].adult);

  uint16_t page = 0;
  recipePreviousPage(page);
  CHECK(page == 0);
  CHECK(recipeNextPage(layout, page) && page == 1);
  CHECK(recipeNextPage(layout, page) && page == 2);
  CHECK(!recipeNextPage(layout, page));  // After the last one: back to the text
  CHECK(page == 2);
  recipePreviousPage(page);
  CHECK(page == 1);

  // A long step takes several pages, each marked with the same step
  std::string words;
  for (int i = 0; i < 60; i++) words += "cook word ";
  CHECK(parse(withSteps(2, words)));
  CHECK(recipeBuildLayout(recipe, INGREDIENT_WIDTH, STEP_WIDTH, fontWidth, nullptr, layout, scratch));
  CHECK(layout.pageCount > 3);
  uint16_t seenLines = 0;
  int8_t lastStep = -2;
  uint8_t expectedSub = 0;
  for (uint16_t i = 0; i < layout.pageCount; i++) {
    const RecipePage& p = layout.pages[i];
    CHECK(p.firstLine == seenLines);  // Every line on exactly one page, in order
    CHECK(p.lineCount >= 1 && p.lineCount <= RECIPE_PAGE_LINES);
    seenLines = (uint16_t)(seenLines + p.lineCount);
    if (p.step != lastStep) { lastStep = p.step; expectedSub = 0; }
    CHECK(p.subPage == expectedSub && p.subPage < p.subPages);
    expectedSub++;
    if (p.step >= 0) CHECK(p.adult);
  }
  CHECK(seenLines == layout.lineCount);

  // More lines than the tables hold: false, so nothing is silently left out
  std::string many;
  for (int i = 0; i < 2000; i++) many += "a ";
  CHECK(parse(withSteps(2, many)));
  CHECK(!recipeBuildLayout(recipe, INGREDIENT_WIDTH, STEP_WIDTH, narrowWidth, nullptr, layout, scratch));
}

// The files the stick will really read: the generated set is the fixture
static void testGeneratedSet(const std::string& dir) {
  DIR* d = opendir(dir.c_str());
  if (!d) {
    printf("  no generated recipes in %s yet\n", dir.c_str());
    return;
  }
  std::vector<std::string> names;
  while (dirent* e = readdir(d)) {
    const std::string name = e->d_name;
    if (name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) names.push_back(name);
  }
  closedir(d);
  int mostPages = 0, adultSteps = 0, steps = 0;
  std::string worst;
  for (const std::string& name : names) {
    const std::string body = readFile(dir + "/" + name);
    CHECK(body.size() < RECIPE_BODY_LIMIT / 2);
    const bool ok = parse(body);
    if (!ok) fprintf(stderr, "  refused: %s\n", name.c_str());
    CHECK(ok);
    if (!ok) continue;
    CHECK(name.find(std::string(".") + recipe.lang + ".json") != std::string::npos);
    CHECK(recipe.stepCount >= 2 && recipe.ingredientCount >= 2);
    const bool laid = recipeBuildLayout(recipe, INGREDIENT_WIDTH, STEP_WIDTH, fontWidth, nullptr, layout, scratch);
    if (!laid) fprintf(stderr, "  no layout: %s\n", name.c_str());
    CHECK(laid);
    if (!laid) continue;
    for (uint16_t i = 0; i < layout.lineCount; i++) {
      const RecipeLine& line = layout.lines[i];
      CHECK(fontWidth(recipe.pool + line.start, line.length, nullptr) <= STEP_WIDTH);
      // Everything shown is inside the font's range
      const char* p = recipe.pool + line.start;
      const char* end = p + line.length;
      while (p < end) {
        uint32_t code;
        decode(p, end, code);
        CHECK(code >= 0x20 && code <= 0x5FF);
      }
    }
    if (layout.pageCount > 32) printf("  too long: %s has %d pages\n", name.c_str(), layout.pageCount);
    if (layout.pageCount > mostPages) { mostPages = layout.pageCount; worst = name; }
    for (uint16_t i = 0; i < layout.pageCount; i++) {
      if (layout.pages[i].subPage == 0 && layout.pages[i].step >= 0) {
        steps++;
        if (layout.pages[i].adult) adultSteps++;
      }
    }
  }
  CHECK(!names.empty());
  CHECK(mostPages <= 32);  // Nobody clicks through more than this on a screen this small

  // Every name in the index leads to the files it promises
  const std::string index = readFile(dir + "/index.txt");
  CHECK(index.size() < RECIPE_BODY_LIMIT);
  size_t start = 0, lines = 0;
  while (start < index.size()) {
    size_t end = index.find('\n', start);
    if (end == std::string::npos) end = index.size();
    const std::string line = index.substr(start, end - start);
    start = end + 1;
    lines++;
    const std::string name = line.substr(0, line.find('|'));
    char slug[RECIPE_SLUG_SIZE], langs[RECIPE_LANGS_SIZE], norm[64];
    CHECK(recipeNormalizeQuery(name.c_str(), norm, sizeof norm) && name == norm);  // Same rule as the generator's
    CHECK(recipeIndexLookup(index.data(), index.size(), norm, slug, sizeof slug, langs, sizeof langs));
    for (const char* lang : {"de", "en"}) {
      if (recipeHasWord(langs, lang)) {
        FILE* f = fopen((dir + "/" + slug + "." + lang + ".json").c_str(), "rb");
        CHECK(f != nullptr);
        if (f) fclose(f);
      }
    }
    CHECK(recipeChooseLang(langs, "DE") != nullptr);
  }
  printf("  generated set: %zu files, %zu names, longest %s with %d pages, %d of %d steps marked\n", names.size(), lines,
         worst.c_str(), mostPages, adultSteps, steps);
}

int main(int argc, char** argv) {
  (void)argc;
  testQuery();
  testIndex();
  testAlcohol();
  testAdult();
  testParse();
  testWrap();
  testLayout();
  testGeneratedSet(argc > 2 ? argv[2] : "../../webpage2/data/recipe");
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
