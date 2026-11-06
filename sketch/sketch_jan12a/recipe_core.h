// Pure logic of the recipe view: finding a dish, reading a recipe file, safety keywords, wrapping and paging.
// No Arduino includes (ArduinoJson works on its own), tested on the host: tests/recipe/run.sh
// The keyword lists are also read by scripts/make_recipe_data.py, so keep their one-string-per-entry form.
#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RECIPE_BODY_LIMIT (16 * 1024)  // Largest file accepted; the generator stays below it
#define RECIPE_POOL_SIZE (12 * 1024)   // All text of one recipe
#define RECIPE_TITLE_SIZE 64
#define RECIPE_SLUG_SIZE 40
#define RECIPE_LANGS_SIZE 8
#define RECIPE_MAX_INGREDIENTS 40
#define RECIPE_MAX_STEPS 40
#define RECIPE_MAX_LINES 700
#define RECIPE_MAX_PAGES 260
#define RECIPE_PAGE_LINES 4  // What fits between the header and the bottom line at a readable size
#define RECIPE_STEP_WIDTH 232        // Text width in pixels on the 240 wide screen
#define RECIPE_INGREDIENT_WIDTH 222  // Leaves room for the bullet

// Alcohol: such a recipe isn't shown at all. Whole words, because "rum" is inside "herum" and "wein" inside
// "Schwein". Wine vinegar and cider vinegar count too: when in doubt, it stays out.
static const char* const RECIPE_ALCOHOL_WORDS[] = {
    "alcohol", "ale",    "amaretto", "beer",     "beers",  "bourbon", "brandy", "champagne",
    "cider",   "cognac", "gin",      "lager",    "liquor", "marsala", "mirin",  "rum",
    "sake",    "sherry", "stout",    "tequila",  "vermouth", "vodka", "whiskey", "whisky",
    "wine",    "wines",  "alkohol",  "bier",     "biere",  "grappa",  "ouzo",   "schnaps",
    "sekt",    "wein",   "weine",    "weinen",   "weines", "punsch"};
// Inside compounds, as German builds them
static const char* const RECIPE_ALCOHOL_PARTS[] = {
    "liqueur", "likör",     "rotwein",   "weißwein",  "weisswein", "apfelwein", "glühwein", "gluehwein",
    "süßwein", "kochwein",  "weinbrand", "weinessig", "bierteig",  "bierbrot",  "weizenbier", "malzbier",
    "rumaroma", "rumrosine", "rumtopf",  "kirschwasser", "eierpunsch"};

// Hot, sharp or oven work: "ask an adult". Over-marking is the safe direction, so stems are broad. Whole words only
// where a stem would hit harmless words: heat in wheat, pan in pancake, pot in potato, hot in shot, cut in biscuit.
static const char* const RECIPE_ADULT_WORDS[] = {
    "bake",   "bakes",  "baked",   "baking",  "cook",   "cooks",   "cooked",  "cooking", "cooker",  "cut",
    "cuts",   "cutting", "cutter", "dice",    "dices",  "diced",   "dicing",  "fried",   "fries",   "griddle",
    "heat",   "heats",  "heated",  "heating", "hob",    "hot",     "mince",   "minces",  "minced",  "mincing",
    "oven",   "ovens",  "pan",     "pans",    "pot",    "pots",    "saucepan", "skillet", "sear",   "sears",
    "seared", "searing", "warm",   "warms",   "warmed", "warming",
    "garen",  "heiß",   "heiße",   "heißem",  "heißen", "heißer",  "heißes",  "heiss",   "heisse",  "heissem",
    "heissen", "heisser", "heisses"};
static const char* const RECIPE_ADULT_PARTS[] = {
    "blend",   "boil",    "broil",   "burn",    "chop",    "flam",    "fry",     "grat",    "grill",    "knife",
    "knive",   "melt",    "microwav", "peel",   "preheat", "processor", "reheat", "roast",  "saut",     "scald",
    "scissor", "simmer",  "slic",    "steam",   "stove",   "toast",
    "ofen",    "back",    "brat",    "brät",    "koch",    "köchel",  "sied",    "frittier", "erhitz",  "hitze",
    "herd",    "erwärm",  "aufwärm", "schmelz", "zerlass", "dünst",   "dämpf",   "röst",    "pfanne",   "topf",
    "messer",  "schere",  "schneid", "schnitt", "hack",    "hobel",   "reibe",   "reiben",  "gerieben", "schäl",
    "würfel",  "mixer",   "pürier",  "blanchier", "mikrowelle"};
// These contain a keyword but mean nothing hot or sharp
static const char* const RECIPE_ADULT_EXCEPTIONS[] = {"baking powder", "baking soda", "backpulver", "messerspitze"};

struct RecipeData {
  char title[RECIPE_TITLE_SIZE];
  char lang[3];
  uint16_t ingredientCount;
  uint16_t stepCount;
  uint16_t ingredient[RECIPE_MAX_INGREDIENTS];  // Offsets into pool
  uint16_t step[RECIPE_MAX_STEPS];
  char pool[RECIPE_POOL_SIZE];
};

struct RecipeLine {
  uint16_t start;  // Offset into pool
  uint16_t length;
  bool itemStart;  // First line of an ingredient: gets the bullet
};

struct RecipePage {
  uint16_t firstLine;
  uint8_t lineCount;
  uint8_t subPage;    // 0-based within its step, or within the ingredients
  uint8_t subPages;   // How many pages this step, or the ingredients, takes
  int8_t step;        // 0-based step, or -1 for the ingredients
  bool adult;         // Step text names hot, sharp or oven work
};

struct RecipeLayout {
  RecipeLine lines[RECIPE_MAX_LINES];
  RecipePage pages[RECIPE_MAX_PAGES];
  uint16_t lineCount;
  uint16_t pageCount;
};

// Width of a piece of UTF-8 text in pixels: the sprite's font on the device, a table in the tests
typedef int (*RecipeMeasure)(const char* text, size_t length, void* context);

// Lower case for ASCII and the German capitals. In UTF-8 the case of Ä, Ö, Ü differs only in the second byte.
inline void recipeLowerInto(const char* in, char* out, size_t capacity) {
  size_t n = 0;
  for (size_t i = 0; in[i] && n + 1 < capacity; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c >= 'A' && c <= 'Z') {
      c = (unsigned char)(c + 32);
    } else if (c == 0xC3 && ((unsigned char)in[i + 1] == 0x84 || (unsigned char)in[i + 1] == 0x96 ||
                             (unsigned char)in[i + 1] == 0x9C)) {
      if (n + 2 >= capacity) break;
      out[n++] = (char)c;
      out[n++] = (char)((unsigned char)in[i + 1] + 0x20);
      i++;
      continue;
    }
    out[n++] = (char)c;
  }
  out[n] = 0;
}

inline bool recipeIsLetter(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}

// True if lowered text has the keyword as a whole word; every German letter byte counts as a letter
inline bool recipeHasWord(const char* lowered, const char* word) {
  const size_t length = strlen(word);
  for (const char* p = strstr(lowered, word); p; p = strstr(p + 1, word)) {
    const bool startOk = p == lowered || !recipeIsLetter((unsigned char)p[-1]);
    if (startOk && !recipeIsLetter((unsigned char)p[length])) return true;
  }
  return false;
}

inline bool recipeTextHasAlcohol(const char* lowered) {
  for (const char* word : RECIPE_ALCOHOL_WORDS) {
    if (recipeHasWord(lowered, word)) return true;
  }
  for (const char* part : RECIPE_ALCOHOL_PARTS) {
    if (strstr(lowered, part)) return true;
  }
  return false;
}

// Takes lowered text and changes it: the harmless phrases are blanked, so "baking powder" isn't baking
inline bool recipeTextNeedsAdult(char* lowered) {
  for (const char* exception : RECIPE_ADULT_EXCEPTIONS) {
    const size_t n = strlen(exception);
    for (char* p = strstr(lowered, exception); p; p = strstr(p + n, exception)) memset(p, ' ', n);
  }
  for (const char* word : RECIPE_ADULT_WORDS) {
    if (recipeHasWord(lowered, word)) return true;
  }
  for (const char* part : RECIPE_ADULT_PARTS) {
    if (strstr(lowered, part)) return true;
  }
  return false;
}

// A question as it stands in the index: lower case, letters and digits, one space between words
inline bool recipeNormalizeQuery(const char* in, char* out, size_t capacity) {
  if (!in || capacity == 0) return false;
  size_t n = 0;
  bool pendingSpace = false;
  for (size_t i = 0; in[i]; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    const bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80;
    if (!keep) {
      pendingSpace = n > 0;
      continue;
    }
    if (pendingSpace) {
      if (n + 1 >= capacity) return false;
      out[n++] = ' ';
      pendingSpace = false;
    }
    if (n + 1 >= capacity) return false;
    out[n++] = (char)c;
  }
  out[n] = 0;
  return n > 0;
}

inline bool recipeSlugValid(const char* slug) {
  if (!slug[0]) return false;
  for (const char* p = slug; *p; p++) {
    const char c = *p;
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

// Copies one '|' field. False if it doesn't fit or the line has none.
inline bool recipeCopyField(const char*& p, const char* lineEnd, char* out, size_t capacity) {
  const char* end = (const char*)memchr(p, '|', lineEnd - p);
  if (!end) end = lineEnd;
  const size_t n = end - p;
  if (n >= capacity) return false;
  memcpy(out, p, n);
  out[n] = 0;
  p = end < lineEnd ? end + 1 : lineEnd;
  return true;
}

// The index has one line per name: "<name>|<slug>|<languages>". Exact match only, since a near match could
// show a different dish. A broken line is skipped; a slug that could leave the folder is refused.
inline bool recipeIndexLookup(const char* body, size_t length, const char* normalized, char* slug, size_t slugSize,
                              char* langs, size_t langsSize) {
  if (!body || !normalized || !normalized[0]) return false;
  const char* const end = body + length;
  const char* p = body;
  char name[64];
  while (p < end) {
    const char* lineEnd = (const char*)memchr(p, '\n', end - p);
    if (!lineEnd) lineEnd = end;
    const char* field = p;
    p = lineEnd + 1;
    if (!recipeCopyField(field, lineEnd, name, sizeof(name)) || strcmp(name, normalized) != 0) continue;
    if (!recipeCopyField(field, lineEnd, slug, slugSize) || !recipeCopyField(field, lineEnd, langs, langsSize)) continue;
    if (recipeSlugValid(slug) && langs[0]) return true;
  }
  return false;
}

// The device's language if the source has it, otherwise the other. nullptr if neither.
inline const char* recipeChooseLang(const char* langs, const char* deviceLang) {
  const bool wantGerman = !deviceLang || strcmp(deviceLang, "EN") != 0;
  const bool hasGerman = recipeHasWord(langs, "de");
  const bool hasEnglish = recipeHasWord(langs, "en");
  if (wantGerman) return hasGerman ? "de" : (hasEnglish ? "en" : nullptr);
  return hasEnglish ? "en" : (hasGerman ? "de" : nullptr);
}

// Copies text into the pool with whitespace made plain. False if empty or out of room.
inline bool recipePoolAdd(RecipeData& out, size_t& used, const char* text, uint16_t& offset) {
  if (!text) return false;
  while (*text == ' ') text++;
  const size_t start = used;
  for (const char* p = text; *p; p++) {
    if (used + 2 > RECIPE_POOL_SIZE) return false;
    const unsigned char c = (unsigned char)*p;
    out.pool[used++] = c < 0x20 || c == 0x7F ? ' ' : (char)c;
  }
  while (used > start && out.pool[used - 1] == ' ') used--;
  if (used == start) return false;
  out.pool[used++] = 0;
  offset = (uint16_t)start;
  return true;
}

// Reads a recipe file. Anything unexpected makes it false: a recipe with a step missing is worse than none,
// so too many steps or too much text refuses the file instead of cutting it. Alcohol refuses it too.
// scratch holds RECIPE_POOL_SIZE bytes: far too much for the stack on the stick.
inline bool recipeParse(const char* body, size_t length, RecipeData& out, char* scratch) {
  memset(&out, 0, offsetof(RecipeData, pool));
  if (!body || length == 0 || length > RECIPE_BODY_LIMIT) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) return false;
  if (!doc.is<JsonObject>()) return false;
  JsonVariantConst titleField = doc["title"];
  JsonVariantConst langField = doc["lang"];
  JsonVariantConst ingredients = doc["ingredients"];
  JsonVariantConst steps = doc["steps"];
  if (!titleField.is<const char*>() || !langField.is<const char*>() || !ingredients.is<JsonArrayConst>() ||
      !steps.is<JsonArrayConst>()) {
    return false;
  }
  const char* title = titleField.as<const char*>();
  const char* lang = langField.as<const char*>();
  if (!title[0] || strlen(title) >= RECIPE_TITLE_SIZE) return false;
  if (strcmp(lang, "de") != 0 && strcmp(lang, "en") != 0) return false;
  JsonArrayConst ingredientList = ingredients.as<JsonArrayConst>();
  JsonArrayConst stepList = steps.as<JsonArrayConst>();
  if (ingredientList.size() == 0 || ingredientList.size() > RECIPE_MAX_INGREDIENTS) return false;
  if (stepList.size() == 0 || stepList.size() > RECIPE_MAX_STEPS) return false;
  strcpy(out.title, title);
  strcpy(out.lang, lang);
  size_t used = 0;
  for (JsonVariantConst item : ingredientList) {
    if (!item.is<const char*>()) return false;
    if (!recipePoolAdd(out, used, item.as<const char*>(), out.ingredient[out.ingredientCount])) return false;
    out.ingredientCount++;
  }
  for (JsonVariantConst item : stepList) {
    if (!item.is<const char*>()) return false;
    if (!recipePoolAdd(out, used, item.as<const char*>(), out.step[out.stepCount])) return false;
    out.stepCount++;
  }
  // Second line of defence: the generator already leaves these out
  recipeLowerInto(out.title, scratch, RECIPE_POOL_SIZE);
  if (recipeTextHasAlcohol(scratch)) return false;
  for (uint16_t i = 0; i < out.ingredientCount + out.stepCount; i++) {
    const uint16_t offset = i < out.ingredientCount ? out.ingredient[i] : out.step[i - out.ingredientCount];
    recipeLowerInto(out.pool + offset, scratch, RECIPE_POOL_SIZE);
    if (recipeTextHasAlcohol(scratch)) return false;
  }
  return true;
}

// Bytes of the UTF-8 character starting here, never past the end of the text
inline size_t recipeCharBytes(const char* p) {
  const unsigned char c = (unsigned char)*p;
  size_t n = c < 0x80 ? 1 : (c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : (c >= 0xC0 ? 2 : 1)));
  for (size_t i = 1; i < n; i++) {
    if (((unsigned char)p[i] & 0xC0) != 0x80) return i;  // Broken or cut off: this also stops at the end
  }
  return n;
}

// Breaks text into lines of at most maxWidth at spaces. A word too long for a line is broken between characters.
// Returns the number of lines added, or -1 if there's no room.
inline int recipeWrap(const char* pool, uint16_t offset, int maxWidth, RecipeMeasure measure, void* context,
                      RecipeLayout& layout) {
  const char* text = pool + offset;
  int added = 0;
  while (*text) {
    while (*text == ' ') text++;
    if (!*text) break;
    size_t bestBreak = 0;  // Last space that still fits
    size_t pos = 0;
    while (text[pos]) {
      const size_t next = pos + recipeCharBytes(text + pos);
      if (measure(text, next, context) > maxWidth) break;
      pos = next;
      if (text[pos] == ' ') bestBreak = pos;
    }
    size_t length;
    if (!text[pos]) {
      length = pos;  // The rest fits
    } else if (bestBreak > 0) {
      length = bestBreak;
    } else {
      // At least one character always goes on the line, or a narrow width would never end
      length = pos > 0 ? pos : recipeCharBytes(text);
    }
    if (layout.lineCount >= RECIPE_MAX_LINES) return -1;
    RecipeLine& line = layout.lines[layout.lineCount++];
    line.start = (uint16_t)(text - pool);
    line.length = (uint16_t)length;
    line.itemStart = added == 0;
    added++;
    text += length;
  }
  return added;
}

inline bool recipeAddPages(RecipeLayout& layout, uint16_t firstLine, uint16_t lines, int8_t step, bool adult) {
  const uint16_t subPages = (uint16_t)((lines + RECIPE_PAGE_LINES - 1) / RECIPE_PAGE_LINES);
  if (subPages == 0 || subPages > 255 || layout.pageCount + subPages > RECIPE_MAX_PAGES) return false;
  for (uint16_t i = 0; i < subPages; i++) {
    RecipePage& page = layout.pages[layout.pageCount++];
    page.firstLine = (uint16_t)(firstLine + i * RECIPE_PAGE_LINES);
    const uint16_t left = (uint16_t)(lines - i * RECIPE_PAGE_LINES);
    page.lineCount = (uint8_t)(left < RECIPE_PAGE_LINES ? left : RECIPE_PAGE_LINES);
    page.subPage = (uint8_t)i;
    page.subPages = (uint8_t)subPages;
    page.step = step;
    page.adult = adult;
  }
  return true;
}

// Lays out the whole recipe: first the ingredients, then each step, as many pages as it takes. Nothing is ever
// left out: if the text doesn't fit the tables, it's false and the caller stays on the text answer.
// scratch holds RECIPE_POOL_SIZE bytes, for the keyword check.
inline bool recipeBuildLayout(const RecipeData& recipe, int ingredientWidth, int stepWidth, RecipeMeasure measure,
                              void* context, RecipeLayout& layout, char* scratch) {
  layout.lineCount = 0;
  layout.pageCount = 0;
  if (recipe.ingredientCount == 0 || recipe.stepCount == 0) return false;
  const uint16_t ingredientStart = layout.lineCount;
  for (uint16_t i = 0; i < recipe.ingredientCount; i++) {
    if (recipeWrap(recipe.pool, recipe.ingredient[i], ingredientWidth, measure, context, layout) <= 0) return false;
  }
  if (!recipeAddPages(layout, ingredientStart, (uint16_t)(layout.lineCount - ingredientStart), -1, false)) return false;
  for (uint16_t i = 0; i < recipe.stepCount; i++) {
    const uint16_t start = layout.lineCount;
    const int added = recipeWrap(recipe.pool, recipe.step[i], stepWidth, measure, context, layout);
    if (added <= 0) return false;
    recipeLowerInto(recipe.pool + recipe.step[i], scratch, RECIPE_POOL_SIZE);
    const bool adult = recipeTextNeedsAdult(scratch);
    if (!recipeAddPages(layout, start, (uint16_t)added, (int8_t)i, adult)) return false;
  }
  return true;
}

// Button C: true if there's a next page. False after the last one, which goes back to the text.
inline bool recipeNextPage(const RecipeLayout& layout, uint16_t& page) {
  if (page + 1 >= layout.pageCount) return false;
  page++;
  return true;
}

// Button B: stays on the first page
inline void recipePreviousPage(uint16_t& page) {
  if (page > 0) page--;
}
