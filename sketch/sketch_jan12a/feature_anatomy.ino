// Anatomy: real organ meshes reduced to low-poly files, and clearly marked schematic cells. See edu_feature.h.
// Files come from scripts/make_anatomy_data.py; anatomy_core.h checks them before anything is used.

#include "anatomy_core.h"
#include "anatomy_view.h"

static_assert(ANATOMY_VIEW_FOOTER == EDU_FOOTER_HEIGHT, "The preview must draw the same size as the stick");

static const unsigned long ANATOMY_LOG_INTERVAL_MS = 5000;

static uint8_t* anatomyBody = nullptr;      // The file, in PSRAM; the model points into it
static int8_t* anatomyNormals = nullptr;    // PSRAM
static AnatomyModel anatomyModel;
static bool anatomyGerman = true;

static uint16_t anatomyShadeTable[ANATOMY_MAX_PALETTE * ANATOMY_SHADES];
static AnatomyWork anatomyWork = {nullptr, nullptr, nullptr, nullptr, nullptr};
static bool anatomyWorkInternal = false;

static unsigned long anatomyLogStart = 0;
static unsigned long anatomyLogDrawUs = 0;
static unsigned long anatomyLogDrawMaxUs = 0;
static uint16_t anatomyLogDrawnMax = 0;
static int anatomyLogFrames = 0;

static void anatomyUnload() {
  free(anatomyBody);
  anatomyBody = nullptr;
  free(anatomyNormals);
  anatomyNormals = nullptr;
  memset(&anatomyModel, 0, sizeof(anatomyModel));
}

// Fills the model and the bottom line from the file. False on anything unexpected: a picture shows instead.
static bool anatomyFetch(const char* query) {
  anatomyUnload();
  const int index = anatomyFindId(query);
  if (index < 0) {
    Serial.printf("Anatomy lookup '%s': not on the list\n", query);
    return false;
  }
  const char* id = ANATOMY_IDS[index];
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  anatomyBody = (uint8_t*)ps_malloc(ANATOMY_BODY_LIMIT);
  if (!anatomyBody) return false;
  // The file name comes from the closed list, never from the query itself
  const String url = String(EDU_DATA_BASE_URL "anatomy/") + id + ".bin";
  size_t length = 0;
  bool ok = eduHttpGet(url, deadline, (char*)anatomyBody, ANATOMY_BODY_LIMIT, length);
  if (ok) {
    uint8_t* smaller = (uint8_t*)ps_realloc(anatomyBody, length);
    if (smaller) anatomyBody = smaller;
    ok = anatomyParse(anatomyBody, length, id, anatomyModel);
  }
  if (ok) {
    anatomyNormals = (int8_t*)ps_malloc((size_t)anatomyModel.triangleCount * 3);
    ok = anatomyNormals != nullptr;
  }
  if (ok) {
    anatomyComputeNormals(anatomyModel, anatomyNormals);
    Preferences anatomyPrefs;
    anatomyPrefs.begin("settings", true);
    anatomyGerman = anatomyPrefs.getString("language", "DE") != "EN";
    anatomyPrefs.end();
    modelLabel = anatomyGerman ? anatomyModel.labelDe : anatomyModel.labelEn;
    if (anatomyModel.flags & ANATOMY_FLAG_SCHEMATIC) {
      modelSourceName = anatomyGerman ? "Schema" : "Schematic";
    } else {
      modelSourceName = anatomyModel.source;
    }
  } else {
    anatomyUnload();
  }
  Serial.printf("Anatomy lookup '%s': %s, %u triangles, %ld ms left\n", id, ok ? "found" : "not available",
                ok ? anatomyModel.triangleCount : 0, (long)(deadline - millis()));
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

static bool anatomyBegin(M5Canvas& c) {
  (void)c;
  if (!anatomyBody || !anatomyModel.vertices) return false;
  anatomyEnd();
  anatomyWorkInternal = true;
  const size_t vertices = anatomyModel.vertexCount;
  anatomyWork.sx = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.sy = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.depth = (int16_t*)anatomyWorkAlloc(vertices * sizeof(int16_t));
  anatomyWork.next = (uint16_t*)anatomyWorkAlloc(anatomyModel.triangleCount * sizeof(uint16_t));
  anatomyWork.heads = (uint16_t*)anatomyWorkAlloc(ANATOMY_DEPTH_BUCKETS * sizeof(uint16_t));
  if (!anatomyWork.sx || !anatomyWork.sy || !anatomyWork.depth || !anatomyWork.next || !anatomyWork.heads) {
    anatomyEnd();
    return false;
  }
  anatomyBuildShades(anatomyModel, anatomyShadeTable);
  anatomyLogStart = millis();
  anatomyLogFrames = 0;
  anatomyLogDrawUs = anatomyLogDrawMaxUs = 0;
  anatomyLogDrawnMax = 0;
  return true;
}

// A line every few seconds, for "model anatomy:<id>" over serial: is it really 25 frames per second?
static void anatomyLogFrame(unsigned long drawUs, uint16_t drawn) {
  anatomyLogFrames++;
  anatomyLogDrawUs += drawUs;
  if (drawUs > anatomyLogDrawMaxUs) anatomyLogDrawMaxUs = drawUs;
  if (drawn > anatomyLogDrawnMax) anatomyLogDrawnMax = drawn;
  const unsigned long elapsed = millis() - anatomyLogStart;
  if (elapsed < ANATOMY_LOG_INTERVAL_MS) return;
  Serial.printf("anatomy %s triangles=%u drawnMax=%u zoom=%u fps=%.1f drawMs=%.1f drawMaxMs=%.1f work=%s sprite=%s freeHeap=%u\n",
                anatomyModel.id, anatomyModel.triangleCount, anatomyLogDrawnMax, modelZoomStep,
                anatomyLogFrames * 1000.0f / elapsed, anatomyLogDrawUs / 1000.0f / anatomyLogFrames,
                anatomyLogDrawMaxUs / 1000.0f, anatomyWorkInternal ? "internal" : "psram",
                modelViewSpriteInPsram() ? "psram" : "internal", (unsigned)ESP.getFreeHeap());
  anatomyLogStart = millis();
  anatomyLogFrames = 0;
  anatomyLogDrawUs = anatomyLogDrawMaxUs = 0;
  anatomyLogDrawnMax = 0;
}

static void anatomyFrame(M5Canvas& c, float dt) {
  (void)dt;
  const unsigned long start = micros();
  c.fillSprite(TFT_BLACK);
  const int viewHeight = HEIGHT - EDU_FOOTER_HEIGHT;
  c.startWrite();
  const uint16_t drawn = anatomyRender(anatomyModel, anatomyNormals, anatomyShadeTable, modelRotation,
                                       anatomyViewScale(viewHeight, modelZoomStep), ANATOMY_VIEW_CAMERA, WIDTH / 2,
                                       viewHeight / 2, WIDTH, viewHeight, anatomyWork,
                                       [&c](int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
                                         c.fillTriangle(x0, y0, x1, y1, x2, y2, color);
                                       });
  c.endWrite();
  eduDrawFooter(c, 0);
  // A schematic is a drawing of how a textbook describes it, not a measured shape: the screen always says so
  if (anatomyModel.flags & ANATOMY_FLAG_SCHEMATIC) {
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.setTextColor(TFT_BLACK, c.color565(0xF2, 0xB7, 0x05));
    c.drawString(anatomyGerman ? " Vereinfachtes Schema " : " Simplified schematic ", 2, 2);
  }
  anatomyLogFrame(micros() - start, drawn);
}

const EduFeature FEATURE_ANATOMY = {
    "anatomy", "",
    nullptr,  // The prompt rule comes once every file on the list is checked
    true, anatomyFetch, anatomyUnload, anatomyBegin, anatomyFrame, anatomyEnd, nullptr, nullptr, nullptr};
