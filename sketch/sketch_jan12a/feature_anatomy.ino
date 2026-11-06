// Anatomy: real organ meshes reduced to low-poly files, cutaways of what the sources really have inside, and clearly
// marked schematic cells. See edu_feature.h.
// Files come from scripts/make_anatomy_data.py; anatomy_core.h checks them before anything is used.

#include "anatomy_core.h"
#include "anatomy_view.h"

static_assert(ANATOMY_VIEW_FOOTER == EDU_FOOTER_HEIGHT, "The preview must draw the same size as the stick");

static const unsigned long ANATOMY_LOG_INTERVAL_MS = 5000;
static const float ANATOMY_STILL_DPS = 3.0f;  // Same as MODEL_STILL_DPS: below this the shared view spins by itself
static const float ANATOMY_SWAY_RAD_PER_S = 0.9f;

enum { ANATOMY_OUTSIDE = 0, ANATOMY_CUT = 1 };

// One loaded file per view
static uint8_t* anatomyBodies[2] = {nullptr, nullptr};    // The file, in PSRAM; the model points into it
static int8_t* anatomyNormalSets[2] = {nullptr, nullptr};  // PSRAM
static AnatomyModel anatomyModels[2];
static AnatomyViews anatomyViews = {false, false, false, false};
static bool anatomyGerman = true;

static uint16_t anatomyShadeTable[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
static AnatomyWork anatomyWork = {nullptr, nullptr, nullptr, nullptr, nullptr};
static bool anatomyWorkInternal = false;
static Mat3 anatomyHeldTurn;
static float anatomySwayPhase = 0;

static unsigned long anatomyLogStart = 0;
static unsigned long anatomyLogDrawUs = 0;
static unsigned long anatomyLogDrawMaxUs = 0;
static uint16_t anatomyLogDrawnMax = 0;
static int anatomyLogFrames = 0;

static void anatomyUnload() {
  for (int view = 0; view < 2; view++) {
    free(anatomyBodies[view]);
    anatomyBodies[view] = nullptr;
    free(anatomyNormalSets[view]);
    anatomyNormalSets[view] = nullptr;
    memset(&anatomyModels[view], 0, sizeof(anatomyModels[view]));
  }
  anatomyViews = {false, false, false, false};
}

// Downloads and checks one view. The file name comes from the closed list, never from the query itself.
static bool anatomyLoadView(int view, int index, unsigned long deadline) {
  const char* id = ANATOMY_IDS[index];
  uint8_t* body = (uint8_t*)ps_malloc(ANATOMY_BODY_LIMIT);
  if (!body) return false;
  const bool separateCutFile = view == ANATOMY_CUT && ANATOMY_KINDS[index] == ANATOMY_OUTSIDE_AND_CUT;
  const String url = String(EDU_DATA_BASE_URL "anatomy/") + id + (separateCutFile ? "-cut.bin" : ".bin");
  size_t length = 0;
  bool ok = eduHttpGet(url, deadline, (char*)body, ANATOMY_BODY_LIMIT, length);
  if (ok) {
    uint8_t* smaller = (uint8_t*)ps_realloc(body, length);
    if (smaller) body = smaller;
    ok = anatomyParse(body, length, id, view == ANATOMY_CUT, anatomyModels[view]);
  }
  int8_t* normals = nullptr;
  if (ok) {
    normals = (int8_t*)ps_malloc((size_t)anatomyModels[view].triangleCount * 3);
    ok = normals != nullptr;
  }
  if (!ok) {
    free(body);
    memset(&anatomyModels[view], 0, sizeof(anatomyModels[view]));
    return false;
  }
  anatomyComputeNormals(anatomyModels[view], normals);
  anatomyBodies[view] = body;
  anatomyNormalSets[view] = normals;
  return true;
}

// Name and source on the bottom line. A view can come from another source than the other one, so each brings its own.
static void anatomySetFooter() {
  const AnatomyModel& m = anatomyModels[anatomyViews.showingCut ? ANATOMY_CUT : ANATOMY_OUTSIDE];
  modelLabel = anatomyGerman ? m.labelDe : m.labelEn;
  if (anatomyViews.showingCut) modelLabel += anatomyGerman ? ", innen" : ", inside";
  if (m.flags & ANATOMY_FLAG_SCHEMATIC) {
    modelSourceName = anatomyGerman ? "Schema" : "Schematic";
  } else {
    modelSourceName = m.source;
  }
}

// Fills the model and the bottom line from the files. False on anything unexpected: a picture shows instead.
static bool anatomyFetch(const char* query) {
  anatomyUnload();
  int index = -1;
  bool wantCut = false;
  if (!anatomyParseQuery(query, index, wantCut)) {
    Serial.printf("Anatomy lookup '%s': not on the list\n", query);
    return false;
  }
  const AnatomyKind kind = ANATOMY_KINDS[index];
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  // What was asked for comes first and has to be there. The other view is a bonus: without it, Button C goes to the text.
  const int asked = wantCut ? ANATOMY_CUT : ANATOMY_OUTSIDE;
  bool ok = anatomyLoadView(asked, index, deadline);
  if (ok && kind == ANATOMY_OUTSIDE_AND_CUT) anatomyLoadView(asked == ANATOMY_CUT ? ANATOMY_OUTSIDE : ANATOMY_CUT, index, deadline);
  if (ok) {
    anatomyViews.hasOutside = anatomyBodies[ANATOMY_OUTSIDE] != nullptr;
    anatomyViews.hasCut = anatomyBodies[ANATOMY_CUT] != nullptr;
    anatomyViews.cutFirst = wantCut;
    anatomyViewsOpen(anatomyViews);
    Preferences anatomyPrefs;
    anatomyPrefs.begin("settings", true);
    anatomyGerman = anatomyPrefs.getString("language", "DE") != "EN";
    anatomyPrefs.end();
    anatomySetFooter();
  } else {
    anatomyUnload();
  }
  Serial.printf("Anatomy lookup '%s': %s, outside %u, cut %u triangles, %ld ms left\n", query, ok ? "found" : "not available",
                anatomyModels[ANATOMY_OUTSIDE].triangleCount, anatomyModels[ANATOMY_CUT].triangleCount, (long)(deadline - millis()));
  return ok;
}

static void anatomyEnd() {
  free(anatomyWork.sx);
  free(anatomyWork.sy);
  free(anatomyWork.depth);
  free(anatomyWork.next);
  free(anatomyWork.heads);
  anatomyWork = {nullptr, nullptr, nullptr, nullptr, nullptr};
}

// Internal RAM is much faster, and these arrays are touched for every triangle in every frame
static void* anatomyWorkAlloc(size_t bytes) {
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (p) return p;
  anatomyWorkInternal = false;
  return ps_malloc(bytes);
}

static void anatomyLogReset() {
  anatomyLogStart = millis();
  anatomyLogFrames = 0;
  anatomyLogDrawUs = anatomyLogDrawMaxUs = 0;
  anatomyLogDrawnMax = 0;
}

// Starts the current view: its colours, its bottom line and a turn that shows it
static void anatomyEnterView() {
  anatomyBuildShades(anatomyModels[anatomyViews.showingCut ? ANATOMY_CUT : ANATOMY_OUTSIDE], anatomyShadeTable);
  anatomySetFooter();
  if (anatomyViews.showingCut) {
    anatomyCutStart(modelRotation);
    anatomyHeldTurn = modelRotation;
    anatomySwayPhase = 0;
  }
  anatomyLogReset();
}

static bool anatomyBegin(M5Canvas& c) {
  (void)c;
  if (!anatomyViews.hasOutside && !anatomyViews.hasCut) return false;
  anatomyEnd();
  anatomyWorkInternal = true;
  // Big enough for both views, so Button C can't run out of memory later
  const size_t vertices = max(anatomyModels[0].vertexCount, anatomyModels[1].vertexCount);
  const size_t triangles = max(anatomyModels[0].triangleCount, anatomyModels[1].triangleCount);
  anatomyWork.sx = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.sy = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.depth = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.next = (uint16_t*)anatomyWorkAlloc(triangles * sizeof(uint16_t));
  anatomyWork.heads = (uint16_t*)anatomyWorkAlloc(ANATOMY_DEPTH_BUCKETS * sizeof(uint16_t));
  if (!anatomyWork.sx || !anatomyWork.sy || !anatomyWork.depth || !anatomyWork.next || !anatomyWork.heads) {
    anatomyEnd();
    return false;
  }
  anatomyViewsOpen(anatomyViews);
  anatomyEnterView();
  return true;
}

// Button C: the other view if there is one, then back to the text
static bool anatomyButtonC() {
  if (!anatomyViewsPressC(anatomyViews)) return false;
  if (!anatomyViews.showingCut) {
    // The same start as a new result: the outside view spins and turns as before
    mat3Identity(modelRotation);
    mat3Rotate(modelRotation, 0.35f, 0.5f, 0.0f);
  }
  anatomyEnterView();
  return true;
}

// A line every few seconds, for "model anatomy:<id>" over serial: is it really 25 frames per second?
static void anatomyLogFrame(const AnatomyModel& m, unsigned long drawUs, uint16_t drawn) {
  anatomyLogFrames++;
  anatomyLogDrawUs += drawUs;
  if (drawUs > anatomyLogDrawMaxUs) anatomyLogDrawMaxUs = drawUs;
  if (drawn > anatomyLogDrawnMax) anatomyLogDrawnMax = drawn;
  const unsigned long elapsed = millis() - anatomyLogStart;
  if (elapsed < ANATOMY_LOG_INTERVAL_MS) return;
  Serial.printf("anatomy %s view=%s triangles=%u drawnMax=%u zoom=%u fps=%.1f drawMs=%.1f drawMaxMs=%.1f work=%s sprite=%s freeHeap=%u\n",
                m.id, anatomyViews.showingCut ? "cut" : "outside", m.triangleCount, anatomyLogDrawnMax, modelZoomStep,
                anatomyLogFrames * 1000.0f / elapsed, anatomyLogDrawUs / 1000.0f / anatomyLogFrames,
                anatomyLogDrawMaxUs / 1000.0f, anatomyWorkInternal ? "internal" : "psram",
                modelViewSpriteInPsram() ? "psram" : "internal", (unsigned)ESP.getFreeHeap());
  anatomyLogReset();
}

// The words beside a cut: each with the colour it stands for. ASCII only, checked in the file.
static void anatomyDrawLegend(M5Canvas& c, const AnatomyModel& m, int viewHeight) {
  c.setFont(&fonts::DejaVu12);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::top_left);
  int y = anatomyLegendTop(viewHeight, m.legendCount, (m.flags & ANATOMY_FLAG_SCHEMATIC) != 0);
  for (int i = 0; i < m.legendCount; i++, y += ANATOMY_LEGEND_LINE) {
    const uint8_t color = anatomyLegendColor(m, i);
    if (color == ANATOMY_LEGEND_NOTE) {
      c.setTextColor(TFT_LIGHTGREY);
      c.drawString(anatomyLegendText(m, i, anatomyGerman), ANATOMY_LEGEND_X, y);
    } else {
      c.fillRect(ANATOMY_LEGEND_X, y + 1, ANATOMY_LEGEND_CHIP, ANATOMY_LEGEND_CHIP, m.palette[color]);
      c.setTextColor(TFT_WHITE);
      c.drawString(anatomyLegendText(m, i, anatomyGerman), ANATOMY_LEGEND_X + ANATOMY_LEGEND_CHIP + ANATOMY_LEGEND_GAP, y);
    }
  }
}

static void anatomyFrame(M5Canvas& c, float dt) {
  const unsigned long start = micros();
  const int view = anatomyViews.showingCut ? ANATOMY_CUT : ANATOMY_OUTSIDE;
  const AnatomyModel& m = anatomyModels[view];
  Mat3 turn = modelRotation;
  if (anatomyViews.showingCut) {
    const float g = modelGyroDps[0] * modelGyroDps[0] + modelGyroDps[1] * modelGyroDps[1] + modelGyroDps[2] * modelGyroDps[2];
    anatomyHoldCut(modelRotation, anatomyHeldTurn, g >= ANATOMY_STILL_DPS * ANATOMY_STILL_DPS);
    turn = modelRotation;
    anatomySwayPhase += dt * ANATOMY_SWAY_RAD_PER_S;
    if (anatomySwayPhase > 2.0f * (float)M_PI) anatomySwayPhase -= 2.0f * (float)M_PI;
    mat3Rotate(turn, 0.0f, sinf(anatomySwayPhase) * ANATOMY_CUT_SWAY_RAD, 0.0f);
  }
  c.fillSprite(TFT_BLACK);
  const int viewHeight = HEIGHT - EDU_FOOTER_HEIGHT;
  c.startWrite();
  const uint16_t drawn = anatomyRender(m, anatomyNormalSets[view], anatomyShadeTable, turn,
                                       anatomyViewScaleFor(viewHeight, m.legendCount, modelZoomStep), ANATOMY_VIEW_CAMERA,
                                       anatomyViewCentreX(WIDTH, m.legendCount, modelZoomStep), viewHeight / 2, WIDTH, viewHeight,
                                       anatomyWork, [&c](int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
                                         c.fillTriangle(x0, y0, x1, y1, x2, y2, color);
                                       });
  c.endWrite();
  if (anatomyViewShowsLegend(m.legendCount, modelZoomStep)) anatomyDrawLegend(c, m, viewHeight);
  eduDrawFooter(c, 0);
  // A schematic is a drawing of how a textbook describes it, not a measured shape: the screen always says so
  if (m.flags & ANATOMY_FLAG_SCHEMATIC) {
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.setTextColor(TFT_BLACK, c.color565(0xF2, 0xB7, 0x05));
    c.drawString(anatomyGerman ? " Vereinfachtes Schema " : " Simplified schematic ", 2, 2);
  }
  anatomyLogFrame(m, micros() - start, drawn);
}

const EduFeature FEATURE_ANATOMY = {
    "anatomy", "",
    "ANATOMIE: Setze 'model' auf 'anatomy:<id>' nur, wenn jemand ausdruecklich fragt, wie ein Koerperteil oder eine Zelle "
    "innen aussieht oder aufgebaut ist, oder ein 3D-Modell davon sehen will. Erlaubte id, genau so geschrieben: heart, brain, "
    "lungs, stomach, liver, kidney, skeleton, skull, eye, hand, animal-cell, plant-cell, red-blood-cell, neuron. Fragen nach "
    "Aufgabe oder Funktion wie 'Was macht das Herz?' und alle anderen Koerperteile: 'model' leer lassen, dann kommt ein Foto. "
    "Niemals Geschlechtsorgane oder nackte Koerper. Liefere nie selbst Daten.",
    true, anatomyFetch, anatomyUnload, anatomyBegin, anatomyFrame, anatomyEnd, nullptr, anatomyButtonC, nullptr};
