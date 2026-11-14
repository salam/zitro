// Dinosaur fact sheets: the picture, the facts, a size comparison and a fun fact, one page each, paged with
// C, B and a tilt as in the timeline. Everything shown comes from scripts/make_dino_data.py (Wikidata,
// quoted Wikipedia sentences, checked Commons pictures) as an index, one sheet and one picture.
// See edu_feature.h, dino_core.h and docs/features/dino.md.
#include "dino_core.h"
#include "timeline_core.h"  // The tilt gesture

// Layout, in pixels. Roboto 10pt reaches about 14 above its baseline and 5 below; DejaVu9 7 and 3.
static const int DINO_MARGIN = 4;
static const int DINO_TEXT_W = WIDTH - 2 * DINO_MARGIN;  // 232: what the tests wrap the facts at
static const int DINO_LINE_H = 17;                       // As TIMELINE_LINE_H
static const int DINO_TITLE_BASELINE = 14;               // The name or heading at the top of a page
static const int DINO_NOTE_BASELINE = 27;                // "Urvogel" and the like, in DejaVu9
static const int DINO_ROWS_BASELINE = 34;                // First fact line under the name...
static const int DINO_ROWS_NOTE_BASELINE = 44;           // ...and under the note: the fifth ends at 116
static const int DINO_BAND_TOP = 97;                     // The dark band on the picture, for the name...
static const int DINO_BAND_NOTE_TOP = 86;                // ...and for the note below it
static const int DINO_BAND_NAME_BASELINE = 112;
static const int DINO_BAND_NAME_NOTE_BASELINE = 101;
static const int DINO_BAND_NOTE_BASELINE = 113;
static const uint8_t DINO_BAND_ALPHA = 184;  // 72 % black over the picture, as in the mock
static const int DINO_SIZE_X = (WIDTH - DINO_SIZE_AREA_W) / 2;  // The area dinoSizeLayout fills
static const int DINO_SIZE_Y = 22;                               // Ends at 101
static const int DINO_PERSON_LABEL_BASELINE = 111;
static const int DINO_BAR_TEXT_MIN_H = 20;  // A bar this high takes its length text inside
static const int DINO_FUN_W = 224;          // As the tests wrap every fun fact
static const int DINO_FUN_X = (WIDTH - DINO_FUN_W) / 2;
static const int DINO_FUN_BASELINE = 36;
static const int DINO_COUNTER_W = 34;

// What the view lays out when it opens, in PSRAM
struct DinoView {
  DinoRow rows[DINO_MAX_ROWS];
  DinoLine lines[DINO_MAX_LINES];
  char fun[DINO_FUN_LINES][DINO_FIELD_LIMIT + 1];
  DinoPageRef pages[DINO_MAX_PAGES];
  int lineCount, pageCount, funLines;
  int column;  // Where the values start, from the left margin: the widest label + 6, as dinoFactLines has it
};

static DinoSheet dinoSheet;
static bool dinoLoaded = false;
static char* dinoSheetText = nullptr;  // The sheet's strings point into it
static uint8_t* dinoJpeg = nullptr;    // Kept until unload: the view may open again without a fetch
static DinoView* dinoView = nullptr;
static M5Canvas* dinoPicture = nullptr;  // Decoded once when the view opens, the band already darkened
static bool dinoEnglish = false;
static int dinoPage = 0;
static int dinoFooterPage = -1;
static bool dinoRedraw = true;  // Nothing moves on a page: it is drawn once, then kept
static TimelineTilt dinoTilt = {0, 0, 0};

static void dinoReadLanguage() {
  Preferences settings;
  settings.begin("settings", true);
  dinoEnglish = settings.getString("language", "DE") == "EN";
  settings.end();
}

static int dinoTextWidth(const char* text, void* context) {
  return ((M5Canvas*)context)->textWidth(text);
}

static void dinoFreeView() {
  if (dinoPicture) {
    dinoPicture->deleteSprite();
    delete dinoPicture;
    dinoPicture = nullptr;
  }
  free(dinoView);
  dinoView = nullptr;
}

static void dinoUnload() {
  dinoFreeView();
  free(dinoSheetText);
  dinoSheetText = nullptr;
  free(dinoJpeg);
  dinoJpeg = nullptr;
  dinoLoaded = false;
}

// Loads one sheet. The index decides first, so what the AI wrote never reaches a URL. The index, the sheet
// and the picture share one deadline. A picture that is missing or not byte for byte the one the sheet names
// is left out, and then the sheet has to be worth showing without it.
static bool dinoFetch(const char* query) {
  dinoUnload();
  const unsigned long start = millis();
  const unsigned long deadline = start + EDU_LOOKUP_DEADLINE_MS;
  char normalised[80], indexFile[16], prefix[3], slug[48];
  if (!dinoIndexName(query, normalised, sizeof(normalised), indexFile, sizeof(indexFile))) {
    Serial.printf("Dino '%s': not a name\n", query);
    return false;
  }
  dinoPrefixOf(normalised, prefix);
  char* index = (char*)ps_malloc(DINO_INDEX_LIMIT);
  if (!index) return false;
  size_t length = 0;
  const bool listed = eduHttpGet(String(EDU_DATA_BASE_URL "dino/") + indexFile, deadline, index, DINO_INDEX_LIMIT,
                                 length) &&
                      dinoResolve(index, length, prefix, normalised, slug, sizeof(slug));
  free(index);
  if (!listed) {
    Serial.printf("Dino '%s': not in %s, or it didn't arrive\n", query, indexFile);
    return false;
  }
  const String base = String(EDU_DATA_BASE_URL "dino/") + slug;
  dinoSheetText = (char*)ps_malloc(DINO_TEXT_LIMIT + 1);  // +1: the parser ends the text in place
  if (!dinoSheetText || !eduHttpGet(base + ".txt", deadline, dinoSheetText, DINO_TEXT_LIMIT, length) ||
      !dinoParseSheet(dinoSheetText, length, DINO_TEXT_LIMIT + 1, slug, dinoSheet)) {
    Serial.printf("Dino '%s': sheet %s not available\n", query, slug);
    dinoUnload();
    return false;
  }
  size_t received = 0;
  if (dinoSheet.hasImage) {
    dinoJpeg = (uint8_t*)ps_malloc(dinoSheet.imageLength);
    if (!dinoJpeg || !eduHttpGet(base + ".jpg", deadline, (char*)dinoJpeg, dinoSheet.imageLength, received) ||
        !dinoImageOk(dinoSheet, dinoJpeg, received)) {
      Serial.printf("Dino '%s': picture missing or damaged (%u of %u bytes)\n", query, (unsigned)received,
                    (unsigned)dinoSheet.imageLength);
      free(dinoJpeg);
      dinoJpeg = nullptr;
      dinoSheet.hasImage = false;
    }
  }
  if (!dinoWorthShowing(dinoSheet)) {
    Serial.printf("Dino '%s': %s has too little to show without its picture\n", query, slug);
    dinoUnload();
    return false;
  }
  dinoLoaded = true;
  dinoPage = 0;
  modelLabel = dinoSheet.name;
  modelSourceName = "Wikidata";
  Serial.printf("Dino '%s': %s, %d rows, picture %s, in %lu ms, free PSRAM %u\n", query, slug,
                dinoRowCount(dinoSheet), dinoSheet.hasImage ? "yes" : "no", millis() - start,
                (unsigned)ESP.getFreePsram());
  return true;
}

static bool dinoHasNote() {
  return dinoGroupNote(dinoSheet.group, dinoEnglish)[0] != 0;
}

// The picture into its own sprite, centred as the timeline does it, with the band for the name darkened
// into it. False if there's no memory or it doesn't decode: then the sheet goes on without its picture page.
static bool dinoDecodePicture() {
  if (!dinoSheet.hasImage || !dinoJpeg) return false;
  const unsigned long start = millis();
  dinoPicture = new M5Canvas();
  dinoPicture->setPsram(true);
  bool ok = dinoPicture->createSprite(DINO_IMAGE_W, DINO_IMAGE_H);
  if (ok) {
    dinoPicture->fillSprite(TFT_BLACK);
    ok = dinoPicture->drawJpg(dinoJpeg, dinoSheet.imageLength, (DINO_IMAGE_W - dinoSheet.imageW) / 2,
                              (DINO_IMAGE_H - dinoSheet.imageH) / 2);
  }
  if (!ok) {
    Serial.printf("Dino: can't show the picture of %s\n", dinoSheet.slug);
    dinoPicture->deleteSprite();
    delete dinoPicture;
    dinoPicture = nullptr;
    return false;
  }
  const int top = dinoHasNote() ? DINO_BAND_NOTE_TOP : DINO_BAND_TOP;
  dinoPicture->fillRectAlpha(0, top, DINO_IMAGE_W, DINO_IMAGE_H - top, DINO_BAND_ALPHA, TFT_BLACK);
  Serial.printf("Dino: picture decoded in %lu ms\n", millis() - start);
  return true;
}

static bool dinoBegin(M5Canvas& c) {
  if (!dinoLoaded) return false;
  dinoFreeView();
  dinoReadLanguage();
  dinoView = (DinoView*)ps_malloc(sizeof(DinoView));
  if (!dinoView) return false;
  const bool picture = dinoDecodePicture();
  DinoSheet shown = dinoSheet;
  shown.hasImage = picture;  // A picture that doesn't decode gets no page
  if (!dinoWorthShowing(shown)) {
    // It was worth showing for its picture: without it, one row alone is not a sheet
    Serial.printf("Dino: %s has too little to show without its picture\n", dinoSheet.slug);
    dinoFreeView();
    return false;
  }
  DinoView& v = *dinoView;
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  const int rowCount = dinoFactRows(dinoSheet, dinoEnglish, v.rows, DINO_MAX_ROWS);
  v.column = 0;
  for (int i = 0; i < rowCount; i++) {
    const int w = c.textWidth(v.rows[i].label);
    if (w > v.column) v.column = w;
  }
  v.column += 6;
  v.lineCount = dinoFactLines(v.rows, rowCount, DINO_TEXT_W, dinoTextWidth, &c, v.lines, DINO_MAX_LINES);
  v.pageCount = dinoPages(shown, v.lines, v.lineCount, v.pages, DINO_MAX_PAGES);
  v.funLines = dinoWrapText(dinoText(dinoSheet.fun, dinoEnglish), DINO_FUN_W, dinoTextWidth, &c, &v.fun[0][0],
                            DINO_FUN_LINES, sizeof(v.fun[0]));
  if (v.funLines > DINO_FUN_LINES) {
    Serial.printf("Dino: the fun fact of %s needs %d lines\n", dinoSheet.slug, v.funLines);
    v.funLines = DINO_FUN_LINES;  // The last one ends in "..."
  }
  if (v.pageCount == 0) {
    dinoFreeView();
    return false;
  }
  if (dinoPage >= v.pageCount) dinoPage = 0;
  dinoTilt = {0, 0, 0};
  dinoFooterPage = -1;
  dinoRedraw = true;
  return true;
}

static void dinoEnd() {
  dinoFreeView();
  if (dinoLoaded) {
    modelLabel = dinoSheet.name;
    modelSourceName = "Wikidata";
  }
  dinoFooterPage = -1;
}

static void dinoGo(int page) {
  if (!dinoView || page < 0 || page >= dinoView->pageCount || page == dinoPage) return;
  dinoPage = page;
  dinoRedraw = true;
}

static void dinoText10(M5Canvas& c, const char* text, int x, int baseline, uint16_t color) {
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(color);
  c.drawString(text, x, baseline);
}

static void dinoTextSmall(M5Canvas& c, const char* text, int x, int baseline, uint16_t color) {
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(color);
  c.drawString(text, x, baseline);
}

// The name in citrus. A name too long for the line takes a smaller font, which only has ASCII.
static void dinoDrawName(M5Canvas& c, int baseline) {
  const char* name = dinoSheet.name;
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  bool ascii = true;
  for (const char* p = name; *p; p++) {
    if ((uint8_t)*p >= 0x80) ascii = false;  // Cut at the edge rather than drawn with missing letters
  }
  if (c.textWidth(name) <= DINO_TEXT_W || !ascii) {
    dinoText10(c, name, DINO_MARGIN, baseline, CITRUS);
    return;
  }
  c.setFont(&fonts::DejaVu12);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(CITRUS);
  c.drawString(name, DINO_MARGIN, baseline);
}

static void dinoDrawNote(M5Canvas& c, int baseline) {
  const char* note = dinoGroupNote(dinoSheet.group, dinoEnglish);
  if (note[0]) dinoTextSmall(c, note, DINO_MARGIN, baseline, TFT_LIGHTGREY);
}

static void dinoDrawPicturePage(M5Canvas& c) {
  dinoPicture->pushSprite(&c, 0, 0);
  const bool note = dinoHasNote();
  dinoDrawName(c, note ? DINO_BAND_NAME_NOTE_BASELINE : DINO_BAND_NAME_BASELINE);
  dinoDrawNote(c, DINO_BAND_NOTE_BASELINE);
}

static void dinoDrawFactsPage(M5Canvas& c, const DinoPageRef& page) {
  const bool note = dinoHasNote();
  dinoDrawName(c, DINO_TITLE_BASELINE);
  dinoDrawNote(c, DINO_NOTE_BASELINE);
  const uint16_t grey = c.color565(0x9A, 0x9A, 0x9A);
  int baseline = note ? DINO_ROWS_NOTE_BASELINE : DINO_ROWS_BASELINE;
  for (int i = 0; i < page.lineCount; i++, baseline += DINO_LINE_H) {
    const DinoLine& line = dinoView->lines[page.firstLine + i];
    if (line.label[0]) dinoText10(c, line.label, DINO_MARGIN, baseline, grey);
    dinoText10(c, line.value, DINO_MARGIN + dinoView->column, baseline, TFT_WHITE);
  }
}

// A person of 170 cm: a head and a body as tall as the layout says, or a block when that is too small
static void dinoDrawPerson(M5Canvas& c, int x, int y, int w, int h) {
  if (h < 9 || w < 3) {
    c.fillRect(x, y, w, h, TFT_WHITE);
    return;
  }
  const int head = min(w, h / 3);
  const int r = (head - 1) / 2;
  c.fillCircle(x + w / 2, y + r, r, TFT_WHITE);
  c.fillRoundRect(x, y + head + 1, w, h - head - 1, min(3, w / 3), TFT_WHITE);
}

// The person beside a citrus bar at one scale; the uncertain end of a range is darker. The length goes on
// the bar where it fits, else above it, else beside it.
static void dinoDrawSizePage(M5Canvas& c) {
  DinoSizeLayout l;
  if (!dinoSizeLayout(dinoSheet, DINO_SIZE_AREA_W, DINO_SIZE_AREA_H, l)) return;
  const char* heading = l.lineOnly ? (dinoEnglish ? "How long it was" : "So lang war er")
                                   : (dinoEnglish ? "How big it was" : "So gross war er");
  dinoText10(c, heading, DINO_SIZE_X, DINO_TITLE_BASELINE, CITRUS);

  const int px = DINO_SIZE_X + l.personX, py = DINO_SIZE_Y + l.personY;
  dinoDrawPerson(c, px, py, l.personW, l.personH);
  const char* person = dinoEnglish ? "Person" : "Mensch";
  c.setFont(&fonts::DejaVu9);
  const int personTextX = max(2, px + l.personW / 2 - (int)c.textWidth(person) / 2);
  dinoTextSmall(c, person, personTextX, DINO_PERSON_LABEL_BASELINE, TFT_WHITE);

  const int bx = DINO_SIZE_X + l.bodyX, by = DINO_SIZE_Y + l.bodyY;
  const int sure = l.bodyW - l.rangeW;
  c.fillRect(bx, by, sure, l.bodyH, CITRUS);
  if (l.rangeW > 0) c.fillRect(bx + sure, by, l.rangeW, l.bodyH, c.color565(0x7A, 0x5F, 0x10));

  const DinoRange& span = l.wingspan ? dinoSheet.wingspan : dinoSheet.length;
  char range[40], text[64];
  dinoFormatLength(span.lo, span.hi, dinoEnglish, range, sizeof(range));
  if (l.wingspan) {
    snprintf(text, sizeof(text), dinoEnglish ? "%s wingspan" : "%s Spannweite", range);
  } else {
    snprintf(text, sizeof(text), dinoEnglish ? "%s long" : "%s lang", range);
  }
  c.setFreeFont(FontManager::getFont12pt());
  const int tw = c.textWidth(text);
  if (l.bodyH >= DINO_BAR_TEXT_MIN_H && sure >= tw + 8) {
    dinoText10(c, text, bx + (sure - tw) / 2, by + l.bodyH / 2 + 5, TFT_BLACK);
    return;
  }
  int x, baseline;
  if (l.bodyY >= 19) {
    x = bx + l.bodyW / 2 - tw / 2;
    baseline = by - 5;
  } else {
    x = bx + l.bodyW + 6;  // A tall bar that fills the area: beside it
    baseline = by + 14;
  }
  if (x < bx) x = bx;
  if (x + tw > WIDTH - DINO_MARGIN) x = WIDTH - DINO_MARGIN - tw;
  dinoText10(c, text, x, baseline, TFT_WHITE);
}

static void dinoDrawFunPage(M5Canvas& c) {
  dinoText10(c, dinoEnglish ? "Did you know?" : "Wusstest du?", DINO_FUN_X, DINO_TITLE_BASELINE, CITRUS);
  for (int i = 0; i < dinoView->funLines; i++) {
    dinoText10(c, dinoView->fun[i], DINO_FUN_X, DINO_FUN_BASELINE + i * DINO_LINE_H, TFT_WHITE);
  }
}

// The bottom line names the picture's maker on the picture page, the fun fact's article on its page and the
// sheet's sources on the others. The page number sits on the right.
static void dinoDrawFooter(M5Canvas& c) {
  if (dinoFooterPage != dinoPage) {
    dinoFooterPage = dinoPage;
    const DinoPage kind = dinoView->pages[dinoPage].kind;
    const char* label = dinoSheet.source[0] ? dinoSheet.source : "Wikidata";
    if (kind == DINO_PAGE_PICTURE) {
      label = dinoSheet.credit[0] ? dinoSheet.credit : "Wikimedia Commons";
      // Too wide for the line: the licence alone, rather than the maker cut off before it (credits.html has all)
      c.setFont(&fonts::DejaVu9);
      c.setTextSize(1);
      if (c.textWidth(label) > WIDTH - DINO_COUNTER_W - 6) label = dinoCreditLicence(label);
    }
    if (kind == DINO_PAGE_FUN && dinoSheet.funSource[0]) label = dinoSheet.funSource;
    modelLabel = label;
    modelSourceName = "";
  }
  eduDrawFooter(c, DINO_COUNTER_W);
  char counter[12];
  snprintf(counter, sizeof(counter), "%d/%d", dinoPage + 1, dinoView->pageCount);
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_right);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(counter, WIDTH - 2, HEIGHT - 4);
}

static void dinoFrame(M5Canvas& c, float dt) {
  if (!dinoView || !dinoLoaded) return;
  const int step = timelineTiltUpdate(dinoTilt, TIMELINE_TILT_SIGN * modelGyroDps[2], dt);
  if (step != 0) dinoGo(dinoPage + step);  // Stops at both ends: a wrist never leaves the view
  if (modelViewRedraw) dinoRedraw = true;  // The button tabs went away
  if (!dinoRedraw) return;
  dinoRedraw = false;
  c.fillSprite(TFT_BLACK);
  const DinoPageRef& page = dinoView->pages[dinoPage];
  switch (page.kind) {
    case DINO_PAGE_PICTURE: dinoDrawPicturePage(c); break;
    case DINO_PAGE_FACTS: dinoDrawFactsPage(c, page); break;
    case DINO_PAGE_SIZE: dinoDrawSizePage(c); break;
    case DINO_PAGE_FUN: dinoDrawFunPage(c); break;
  }
  dinoDrawFooter(c);
}

static void dinoButtonB() {
  dinoGo(dinoPage - 1);
}

// Next page. Past the last one: back to the text, and the next visit starts from the first page.
static bool dinoButtonC() {
  if (dinoView && dinoPage < dinoView->pageCount - 1) {
    dinoGo(dinoPage + 1);
    return true;
  }
  dinoPage = 0;
  return false;
}

// B: previous page, except on the first. C: next, or the written answer after the last.
static void dinoHints(int& kindB, int& kindC) {
  kindB = dinoPage > 0 ? HINT_EARLIER : HINT_NONE;
  kindC = (dinoView && dinoPage < dinoView->pageCount - 1) ? HINT_LATER : HINT_TEXT;
}

static const char DINO_PROMPT_RULE[] =
    "DINOSAURIER: Setze 'model' auf 'dino:<Gattung>' wenn der Nutzer nach einem bestimmten Dinosaurier, Flugsaurier, "
    "Meeresreptil aus der Dinosaurierzeit oder Urvogel fragt (z.B. 'Erzaehl mir vom T-Rex' ergibt 'dino:Tyrannosaurus', "
    "'Wie gross war ein Brachiosaurus?' ergibt 'dino:Brachiosaurus'). Bei allgemeinen Fragen ('Warum starben die "
    "Dinosaurier aus?') bleibt model leer. Beantworte die Frage im Text wie gewohnt, nenne dort aber keine Laengen, "
    "Groessen, Gewichte, Geschwindigkeiten oder Jahreszahlen: die zeigt Zitro belegt in einem Steckbrief. "
    "Erwaehne den Steckbrief nicht selbst. Fuelle image_query auch dann wie immer.";

const EduFeature FEATURE_DINO = {"dino",     "Wikidata", DINO_PROMPT_RULE, false,       dinoFetch,
                                 dinoUnload, dinoBegin,  dinoFrame,        dinoEnd,     dinoButtonB,
                                 dinoButtonC, nullptr,   nullptr,          dinoHints};
