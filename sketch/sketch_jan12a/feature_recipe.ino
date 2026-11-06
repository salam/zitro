// Recipes: ingredients, then one step per screen. Prepared from Wikibooks on the Zitro server, so every word comes
// from a cookbook page and the AI only names the dish. See edu_feature.h and docs/features/recipe.md.
// Function signatures use plain types only: prototypes of .ino functions land before recipe_core.h is known.

#include "recipe_core.h"

static const int RECIPE_HEADER_BASELINE = 15;
static const int RECIPE_FIRST_BASELINE = 37;
static const int RECIPE_LINE_HEIGHT = 23;  // The font's own line height: 4 lines between header and bottom line
static const int RECIPE_TEXT_X = 4;
static const int RECIPE_BULLET_X = 5;
static const int RECIPE_INGREDIENT_X = 12;
static const int RECIPE_PAGE_NUMBER_ROOM = 32;
static const size_t RECIPE_LINE_BUFFER = 160;
// A step takes longer to do than the 30 seconds until the screen goes off. Bounded, so a stick left lying still sleeps.
static const unsigned long RECIPE_READ_MS = 5UL * 60UL * 1000UL;

static RecipeData* recipeData = nullptr;      // From fetch until unload
static RecipeLayout* recipeLayout = nullptr;  // While the view is open
static uint16_t recipePage = 0;
static bool recipeGerman = true;
static unsigned long recipeLastPress = 0;
static M5Canvas* recipeMeasureCanvas = nullptr;

static int recipeMeasureText(const char* text, size_t length, void* context) {
  (void)context;
  char piece[RECIPE_LINE_BUFFER];
  if (length >= sizeof(piece)) return INT16_MAX;  // No line is this long: it doesn't fit
  memcpy(piece, text, length);
  piece[length] = 0;
  return recipeMeasureCanvas->textWidth(piece);
}

static void recipeUnload() {
  free(recipeData);
  recipeData = nullptr;
  recipePage = 0;
}

static void recipeEnd() {
  free(recipeLayout);
  recipeLayout = nullptr;
  recipeMeasureCanvas = nullptr;
}

static bool recipeBegin(M5Canvas& c) {
  if (!recipeData) return false;
  recipeLayout = (RecipeLayout*)ps_malloc(sizeof(RecipeLayout));
  char* scratch = (char*)ps_malloc(RECIPE_POOL_SIZE);
  bool ok = recipeLayout && scratch;
  if (ok) {
    recipeMeasureCanvas = &c;
    c.setFreeFont(FontManager::getFont12pt());
    c.setTextSize(1);
    ok = recipeBuildLayout(*recipeData, RECIPE_INGREDIENT_WIDTH, RECIPE_STEP_WIDTH, recipeMeasureText, nullptr,
                           *recipeLayout, scratch);
  }
  free(scratch);
  if (!ok) {
    recipeEnd();
    return false;
  }
  if (recipePage >= recipeLayout->pageCount) recipePage = 0;
  recipeLastPress = millis();
  return true;
}

// Hot, sharp or oven work, from keywords in this step's text
static void recipeDrawAdultMarker(M5Canvas& c, int headerRight) {
  const char* text = recipeGerman ? "Erwachsene fragen" : "Ask an adult";
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  if (headerRight + c.textWidth(text) + 12 > WIDTH) text = "!";  // Never over the step number
  const int width = c.textWidth(text) + 8;
  const int x = WIDTH - width - 2;
  c.fillRoundRect(x, 2, width, 15, 3, c.color565(0xF2, 0x7A, 0x05));
  c.setTextColor(TFT_BLACK);
  c.setTextDatum(textdatum_t::baseline_left);
  c.drawString(text, x + 4, 13);
}

static void recipeFrame(M5Canvas& c, float dt) {
  (void)dt;
  c.fillSprite(TFT_BLACK);
  if (!recipeData || !recipeLayout || recipePage >= recipeLayout->pageCount) return;
  const RecipePage& page = recipeLayout->pages[recipePage];
  const bool ingredients = page.step < 0;

  char header[32];
  if (ingredients) {
    const char* word = recipeGerman ? "Zutaten" : "Ingredients";
    if (page.subPages > 1) {
      snprintf(header, sizeof(header), "%s %u/%u", word, page.subPage + 1, page.subPages);
    } else {
      snprintf(header, sizeof(header), "%s", word);
    }
  } else {
    snprintf(header, sizeof(header), "%s %d/%u", recipeGerman ? "Schritt" : "Step", page.step + 1,
             recipeData->stepCount);
  }
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(c.color565(0xF2, 0xB7, 0x05));
  c.drawString(header, RECIPE_TEXT_X, RECIPE_HEADER_BASELINE);
  const int headerRight = RECIPE_TEXT_X + c.textWidth(header);

  c.setTextColor(TFT_WHITE);
  char text[RECIPE_LINE_BUFFER];
  for (uint8_t i = 0; i < page.lineCount; i++) {
    const RecipeLine& line = recipeLayout->lines[page.firstLine + i];
    const size_t length = min((size_t)line.length, sizeof(text) - 1);
    memcpy(text, recipeData->pool + line.start, length);
    text[length] = 0;
    const int y = RECIPE_FIRST_BASELINE + i * RECIPE_LINE_HEIGHT;
    if (ingredients && line.itemStart) c.fillCircle(RECIPE_BULLET_X, y - 5, 2, c.color565(0xF2, 0xB7, 0x05));
    c.drawString(text, ingredients ? RECIPE_INGREDIENT_X : RECIPE_TEXT_X, y);
  }

  if (page.adult) {
    recipeDrawAdultMarker(c, headerRight);
  } else if (ingredients) {
    // Required by the licence; here it's on screen without taking room from the steps
    c.setFont(&fonts::DejaVu9);
    c.setTextColor(TFT_DARKGREY);
    c.setTextDatum(textdatum_t::baseline_right);
    c.drawString("CC BY-SA 4.0", WIDTH - 3, 13);
    c.setTextDatum(textdatum_t::baseline_left);
  }

  const bool showPageNumber = !ingredients && page.subPages > 1;
  eduDrawFooter(c, showPageNumber ? RECIPE_PAGE_NUMBER_ROOM : 0);
  if (showPageNumber) {
    char pageNumber[10];
    snprintf(pageNumber, sizeof(pageNumber), "%u/%u", page.subPage + 1, page.subPages);
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    c.setTextColor(TFT_WHITE);
    c.setTextDatum(textdatum_t::baseline_right);
    c.drawString(pageNumber, WIDTH - 3, HEIGHT - 4);
    c.setTextDatum(textdatum_t::baseline_left);
  }
}

static void recipeButtonB() {
  recipeLastPress = millis();
  recipePreviousPage(recipePage);
}

// False after the last page: back to the text, and the next look starts with the ingredients again
static bool recipeButtonC() {
  recipeLastPress = millis();
  if (recipeLayout && recipeNextPage(*recipeLayout, recipePage)) return true;
  recipePage = 0;
  return false;
}

static bool recipeKeepAwake() {
  return millis() - recipeLastPress < RECIPE_READ_MS;
}

// Finds the dish in the index, then loads it in the device's language if the source has that. Anything else is false,
// and Zitro shows the picture.
static bool recipeFetch(const char* query) {
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  char normalized[sizeof(ModelRef::query)];
  if (!recipeNormalizeQuery(query, normalized, sizeof(normalized))) return false;
  Preferences prefs;
  prefs.begin("settings", true);
  const String language = prefs.getString("language", "DE");
  prefs.end();
  recipeGerman = language != "EN";

  char* body = (char*)ps_malloc(RECIPE_BODY_LIMIT);
  char* scratch = (char*)ps_malloc(RECIPE_POOL_SIZE);
  recipeData = (RecipeData*)ps_malloc(sizeof(RecipeData));
  bool ok = body && scratch && recipeData;
  size_t length = 0;
  char slug[RECIPE_SLUG_SIZE];
  char langs[RECIPE_LANGS_SIZE];
  ok = ok && eduHttpGet(EDU_DATA_BASE_URL "recipe/index.txt", deadline, body, RECIPE_BODY_LIMIT, length) &&
       recipeIndexLookup(body, length, normalized, slug, sizeof(slug), langs, sizeof(langs));
  const char* chosen = ok ? recipeChooseLang(langs, language.c_str()) : nullptr;
  ok = chosen != nullptr;
  if (ok) {
    const String url = String(EDU_DATA_BASE_URL "recipe/") + slug + "." + chosen + ".json";
    ok = eduHttpGet(url, deadline, body, RECIPE_BODY_LIMIT, length) && recipeParse(body, length, *recipeData, scratch) &&
         strcmp(recipeData->lang, chosen) == 0;
  }
  if (ok) {
    recipePage = 0;
    modelLabel = recipeData->title;
    // Not in the device's language: the bottom line says so
    const bool other = (strcmp(chosen, "de") == 0) != recipeGerman;
    modelSourceName = other ? (recipeGerman ? "Wikibooks (EN)" : "Wikibooks (DE)") : "Wikibooks";
  }
  free(body);
  free(scratch);
  if (!ok) recipeUnload();  // fetchModel does this too; a failed lookup keeps nothing
  Serial.printf("Recipe lookup '%s': %s, %ld ms left\n", query, ok ? "found" : "not available", (long)(deadline - millis()));
  return ok;
}

const EduFeature FEATURE_RECIPE = {"recipe",       "Wikibooks",   nullptr,      false,         recipeFetch,    recipeUnload,
                                   recipeBegin,    recipeFrame,   recipeEnd,    recipeButtonB, recipeButtonC,  recipeKeepAwake};
