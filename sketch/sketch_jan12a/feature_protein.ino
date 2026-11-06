// Proteins, enzymes and DNA: real backbones from a small set on the Zitro server, made from the
// RCSB PDB by scripts/make_protein_data.py. A name resolves through the index only: the AI can't
// pick a record, and a name that isn't listed gets the picture. See edu_feature.h.

#include "protein_core.h"

static const float PROTEIN_ZOOMS[3] = {1.0f, 1.5f, 2.2f};
static const float PROTEIN_CAMERA = 3.0f;  // Distance from the model's centre, in model radii
static const uint8_t PROTEIN_SHADES = 4;
static const uint8_t PROTEIN_COLOURS = 32;

static ProteinPoint* proteinPoints = nullptr;
static uint16_t proteinPointCount = 0;
static uint16_t proteinChainCount = 0;
static int16_t* proteinSX = nullptr;
static int16_t* proteinSY = nullptr;
static float* proteinZ = nullptr;
static uint16_t* proteinOrder = nullptr;
static uint16_t proteinPalette[PROTEIN_SHADES][PROTEIN_COLOURS];
static unsigned long proteinViewStart = 0;
static uint32_t proteinFrames = 0;

static void proteinUnload() {
  free(proteinPoints);
  proteinPoints = nullptr;
  proteinPointCount = proteinChainCount = 0;
}

static bool proteinDeviceEnglish() {
  Preferences prefs;
  prefs.begin("settings", true);
  const bool english = prefs.getString("language", "DE") == "EN";
  prefs.end();
  return english;
}

static bool proteinFetch(const char* name) {
  proteinUnload();
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  char* body = (char*)ps_malloc(PROTEIN_FILE_LIMIT);
  proteinPoints = (ProteinPoint*)ps_malloc(sizeof(ProteinPoint) * PROTEIN_MAX_POINTS);
  bool ok = body && proteinPoints;
  size_t length = 0;
  ProteinEntry entry;
  ok = ok && eduHttpGet(EDU_DATA_BASE_URL "protein/index.txt", deadline, body, PROTEIN_INDEX_LIMIT, length);
  ok = ok && proteinResolve(body, length, name, entry);
  if (ok) {
    // proteinResolve only passes IDs made of digits and capitals, so this can't leave the folder
    String file = entry.id;
    file.toLowerCase();
    ok = eduHttpGet(String(EDU_DATA_BASE_URL "protein/") + file + ".txt", deadline, body, PROTEIN_FILE_LIMIT, length) &&
         proteinParseStructure(body, length, entry.id, proteinPoints, PROTEIN_MAX_POINTS, proteinPointCount,
                               proteinChainCount);
  }
  free(body);
  if (ok) {
    modelLabel = proteinDeviceEnglish() ? entry.en : entry.de;
    modelSourceName = String("RCSB PDB ") + entry.id;
  } else {
    proteinUnload();
  }
  Serial.printf("Protein lookup '%s': %s, %u points, %u chains, %ld ms left\n", name, ok ? "found" : "not available",
                proteinPointCount, proteinChainCount, (long)(deadline - millis()));
  return ok;
}

static void proteinEnd() {
  if (proteinViewStart && proteinFrames > 1) {
    // For checking the drawing speed on the stick
    Serial.printf("Protein view: %u points, %.1f frames per second\n", proteinPointCount,
                  proteinFrames * 1000.0f / max(1UL, millis() - proteinViewStart));
  }
  proteinViewStart = 0;
  free(proteinSX); proteinSX = nullptr;
  free(proteinSY); proteinSY = nullptr;
  free(proteinZ); proteinZ = nullptr;
  free(proteinOrder); proteinOrder = nullptr;
}

static bool proteinBegin(M5Canvas& c) {
  (void)c;
  if (!proteinPoints || proteinPointCount < 2) return false;
  proteinSX = (int16_t*)ps_malloc(sizeof(int16_t) * proteinPointCount);
  proteinSY = (int16_t*)ps_malloc(sizeof(int16_t) * proteinPointCount);
  proteinZ = (float*)ps_malloc(sizeof(float) * proteinPointCount);
  proteinOrder = (uint16_t*)ps_malloc(sizeof(uint16_t) * proteinPointCount);
  if (!proteinSX || !proteinSY || !proteinZ || !proteinOrder) {
    proteinEnd();
    return false;
  }
  for (uint8_t s = 0; s < PROTEIN_SHADES; s++) {
    for (uint8_t i = 0; i < PROTEIN_COLOURS; i++) {
      proteinPalette[s][i] = proteinShade565(proteinRampColor((uint8_t)(i * 255 / (PROTEIN_COLOURS - 1))), s);
    }
  }
  proteinViewStart = millis();
  proteinFrames = 0;
  return true;
}

// Parallel lines, stepping sideways along the shorter axis so they stay together on the pixel grid
static void proteinDrawThickLine(M5Canvas& c, int x0, int y0, int x1, int y1, int width, uint16_t color) {
  const bool steep = abs(y1 - y0) > abs(x1 - x0);
  const int sx = steep ? 1 : 0, sy = steep ? 0 : 1;
  const int first = -(width - 1) / 2;
  for (int i = 0; i < width; i++) {
    const int o = first + i;
    c.drawLine(x0 + o * sx, y0 + o * sy, x1 + o * sx, y1 + o * sy, color);
  }
}

static void proteinFrame(M5Canvas& c, float dt) {
  (void)dt;
  c.fillSprite(TFT_BLACK);
  const int cx = WIDTH / 2;
  const int cy = (HEIGHT - EDU_FOOTER_HEIGHT) / 2;
  const float zoom = PROTEIN_ZOOMS[modelZoomStep % 3];
  const float scale = (HEIGHT - EDU_FOOTER_HEIGHT) * 0.42f * zoom;
  const uint16_t n = proteinPointCount;
  const int width = proteinLineWidth(n, modelZoomStep);

  float p[3];
  for (uint16_t i = 0; i < n; i++) {
    const ProteinPoint& a = proteinPoints[i];
    rotatePoint(modelRotation, a.x, a.y, a.z, p);
    const float perspective = PROTEIN_CAMERA / (PROTEIN_CAMERA - p[2]);
    proteinSX[i] = (int16_t)(cx + p[0] * scale * perspective);
    proteinSY[i] = (int16_t)(cy - p[1] * scale * perspective);
    proteinZ[i] = p[2];
  }
  // A line lies halfway between its two points. Backwards, so the point before still has its own depth.
  for (uint16_t i = n; i-- > 1;) {
    if (proteinPoints[i].connected) proteinZ[i] = (proteinZ[i] + proteinZ[i - 1]) * 0.5f;
  }
  proteinDepthOrder(proteinZ, n, proteinOrder);

  for (uint16_t k = 0; k < n; k++) {
    const uint16_t i = proteinOrder[k];
    const ProteinPoint& a = proteinPoints[i];
    const float z = proteinZ[i];
    int shade = (int)((z + 1.0f) * 0.5f * PROTEIN_SHADES);
    shade = constrain(shade, 0, PROTEIN_SHADES - 1);
    const uint16_t color = proteinPalette[shade][a.t * (PROTEIN_COLOURS - 1) / 255];
    if (a.connected) {
      proteinDrawThickLine(c, proteinSX[i - 1], proteinSY[i - 1], proteinSX[i], proteinSY[i], width, color);
      // Round joints, where lines are thick enough for the corners to show
      if (width >= 4) c.fillCircle(proteinSX[i], proteinSY[i], width / 2, color);
    } else if (i + 1 >= n || !proteinPoints[i + 1].connected) {
      // A point on its own, cut off by gaps on both sides: real, so it's still shown
      c.fillCircle(proteinSX[i], proteinSY[i], width / 2 + 1, color);
    } else if (width >= 4) {
      c.fillCircle(proteinSX[i], proteinSY[i], width / 2, color);
    }
  }
  eduDrawFooter(c, 0);
  proteinFrames++;
}

const EduFeature FEATURE_PROTEIN = {
    "protein", "RCSB PDB",
    "Setze 'model' auf 'protein:<englischer Name>' NUR wenn nach Aufbau oder Struktur eines Proteins, Enzyms oder der "
    "DNA gefragt wird oder ausdruecklich ein 3D-Modell davon gewuenscht ist (z.B. 'Wie ist Haemoglobin aufgebaut?' "
    "ergibt 'protein:hemoglobin', 'Zeig mir DNA in 3D' ergibt 'protein:dna'). Dafuer nie 'molecule:'. Nenne nur den "
    "gebraeuchlichen Namen, nie eine PDB-Nummer oder andere Daten. Bei 'Was ist ein Enzym?' oder 'Wozu dient Insulin?' "
    "bleibt model leer.",
    true, proteinFetch, proteinUnload, proteinBegin, proteinFrame, proteinEnd, nullptr, nullptr, nullptr};
