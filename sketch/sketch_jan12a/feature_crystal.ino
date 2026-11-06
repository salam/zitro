// Crystals: atoms computed from published unit cells in crystal_table.h, a closed list. A name that
// isn't listed gets the picture. Needs no network. See edu_feature.h.

#include "crystal_core.h"

static const float CRYSTAL_ZOOMS[3] = {1.0f, 1.5f, 2.2f};
static const float CRYSTAL_CAMERA = 3.0f;  // Distance from the model's centre, in model radii
static const uint8_t CRYSTAL_LEGEND_MAX = 4;

static const CrystalEntry* crystalCurrent = nullptr;
static CrystalAtom* crystalAtoms = nullptr;
static uint16_t* crystalAdjStart = nullptr;
static uint16_t* crystalAdj = nullptr;
static uint16_t crystalAtomCount = 0;
static uint16_t crystalBondCount = 0;
static int16_t* crystalSX = nullptr;
static int16_t* crystalSY = nullptr;
static float* crystalZ = nullptr;
static uint16_t* crystalOrder = nullptr;
static uint8_t crystalLegend[CRYSTAL_LEGEND_MAX];
static uint8_t crystalLegendCount = 0;
static int crystalLegendWidth = 0;
static unsigned long crystalViewStart = 0;
static uint32_t crystalFrames = 0;

static void crystalUnload() {
  free(crystalAtoms); crystalAtoms = nullptr;
  free(crystalAdjStart); crystalAdjStart = nullptr;
  free(crystalAdj); crystalAdj = nullptr;
  crystalAtomCount = crystalBondCount = 0;
  crystalCurrent = nullptr;
}

static bool crystalFetch(const char* name) {
  crystalUnload();
  const int found = crystalResolve(name);
  if (found < 0) {
    Serial.printf("Crystal '%s': not in the list\n", name);
    return false;
  }
  const CrystalEntry& entry = CRYSTAL_ENTRIES[found];
  crystalAtoms = (CrystalAtom*)ps_malloc(sizeof(CrystalAtom) * CRYSTAL_MAX_ATOMS);
  crystalAdjStart = (uint16_t*)ps_malloc(sizeof(uint16_t) * (CRYSTAL_MAX_ATOMS + 1));
  crystalAdj = (uint16_t*)ps_malloc(sizeof(uint16_t) * CRYSTAL_MAX_BONDS * 2);
  float angstromPerUnit = 0;
  const bool ok = crystalAtoms && crystalAdjStart && crystalAdj &&
                  crystalBuild(entry, crystalAtoms, CRYSTAL_MAX_ATOMS, crystalAdjStart, crystalAdj, CRYSTAL_MAX_BONDS * 2,
                               crystalAtomCount, crystalBondCount, angstromPerUnit);
  if (!ok) {
    crystalUnload();
    Serial.printf("Crystal '%s': no memory\n", name);
    return false;
  }
  Preferences prefs;
  prefs.begin("settings", true);
  const bool english = prefs.getString("language", "DE") == "EN";
  prefs.end();
  crystalCurrent = &entry;
  modelLabel = english ? entry.en : entry.de;
  modelSourceName = entry.source;
  Serial.printf("Crystal '%s': %s, %u atoms, %u bonds\n", name, entry.key, crystalAtomCount, crystalBondCount);
  return true;
}

static void crystalEnd() {
  if (crystalViewStart && crystalFrames > 1) {
    // For checking the drawing speed on the stick
    Serial.printf("Crystal view: %u atoms, %.1f frames per second\n", crystalAtomCount,
                  crystalFrames * 1000.0f / max(1UL, millis() - crystalViewStart));
  }
  crystalViewStart = 0;
  free(crystalSX); crystalSX = nullptr;
  free(crystalSY); crystalSY = nullptr;
  free(crystalZ); crystalZ = nullptr;
  free(crystalOrder); crystalOrder = nullptr;
}

static bool crystalBegin(M5Canvas& c) {
  if (!crystalAtoms || crystalAtomCount < 2) return false;
  crystalSX = (int16_t*)ps_malloc(sizeof(int16_t) * crystalAtomCount);
  crystalSY = (int16_t*)ps_malloc(sizeof(int16_t) * crystalAtomCount);
  crystalZ = (float*)ps_malloc(sizeof(float) * crystalAtomCount);
  crystalOrder = (uint16_t*)ps_malloc(sizeof(uint16_t) * crystalAtomCount);
  if (!crystalSX || !crystalSY || !crystalZ || !crystalOrder) {
    crystalEnd();
    return false;
  }
  crystalLegendCount = 0;
  for (uint16_t i = 0; i < crystalAtomCount; i++) {
    bool listed = false;
    for (uint8_t k = 0; k < crystalLegendCount; k++) listed = listed || crystalLegend[k] == crystalAtoms[i].element;
    if (!listed && crystalLegendCount < CRYSTAL_LEGEND_MAX) crystalLegend[crystalLegendCount++] = crystalAtoms[i].element;
  }
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  crystalLegendWidth = 0;
  for (uint8_t i = 0; i < crystalLegendCount; i++) crystalLegendWidth += c.textWidth(elementSymbol(crystalLegend[i])) + 3;
  crystalViewStart = millis();
  crystalFrames = 0;
  return true;
}

// One half of a bond, from the atom to the middle, in that atom's colour
static void crystalDrawHalfBond(M5Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  const bool steep = abs(y1 - y0) > abs(x1 - x0);
  const int sx = steep ? 1 : 0, sy = steep ? 0 : 1;
  c.drawLine(x0, y0, x1, y1, color);
  c.drawLine(x0 + sx, y0 + sy, x1 + sx, y1 + sy, color);
}

static void crystalFrame(M5Canvas& c, float dt) {
  (void)dt;
  c.fillSprite(TFT_BLACK);
  const int cx = WIDTH / 2;
  const int cy = (HEIGHT - EDU_FOOTER_HEIGHT) / 2;
  const float zoom = CRYSTAL_ZOOMS[modelZoomStep % 3];
  const float scale = (HEIGHT - EDU_FOOTER_HEIGHT) * 0.42f * zoom;
  const uint16_t n = crystalAtomCount;

  float p[3];
  for (uint16_t i = 0; i < n; i++) {
    const CrystalAtom& a = crystalAtoms[i];
    rotatePoint(modelRotation, a.x, a.y, a.z, p);
    const float perspective = CRYSTAL_CAMERA / (CRYSTAL_CAMERA - p[2]);
    crystalSX[i] = (int16_t)(cx + p[0] * scale * perspective);
    crystalSY[i] = (int16_t)(cy - p[1] * scale * perspective);
    crystalZ[i] = p[2];
  }
  proteinDepthOrder(crystalZ, n, crystalOrder);

  for (uint16_t k = 0; k < n; k++) {
    const uint16_t i = crystalOrder[k];
    const uint16_t color = elementColor565(crystalAtoms[i].element);
    const int x = crystalSX[i], y = crystalSY[i];
    for (uint16_t b = crystalAdjStart[i]; b < crystalAdjStart[i + 1]; b++) {
      const uint16_t other = crystalAdj[b];
      crystalDrawHalfBond(c, x, y, (x + crystalSX[other]) / 2, (y + crystalSY[other]) / 2, color);
    }
    const float perspective = CRYSTAL_CAMERA / (CRYSTAL_CAMERA - crystalZ[i]);
    int r = (int)(crystalAtomRadius(n, crystalAtoms[i].element, modelZoomStep) * perspective + 0.5f);
    if (r < 2) r = 2;
    // A dark rim keeps overlapping atoms apart, which is what makes the depth readable
    c.fillCircle(x, y, r + 1, TFT_BLACK);
    c.fillCircle(x, y, r, color);
  }

  eduDrawFooter(c, crystalLegendWidth);
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  int x = WIDTH - 2;
  for (int i = crystalLegendCount - 1; i >= 0; i--) {
    const char* symbol = elementSymbol(crystalLegend[i]);
    x -= c.textWidth(symbol);
    c.setTextColor(elementColor565(crystalLegend[i]));
    c.drawString(symbol, x, HEIGHT - 4);
    x -= 3;
  }
  crystalFrames++;
}

const EduFeature FEATURE_CRYSTAL = {
    "crystal", "COD",
    "Setze 'model' auf 'crystal:<Wert>' NUR wenn nach dem Kristallgitter oder Aufbau eines dieser Stoffe als Feststoff "
    "gefragt wird oder ausdruecklich ein 3D-Modell davon gewuenscht ist. Werte: salt, diamond, graphite, ice, quartz, "
    "calcite, pyrite, fluorite, copper, gold, silver, iron, silicon; andere gibt es nicht. Dafuer nie 'molecule:' "
    "(z.B. 'Wie ist Kochsalz aufgebaut?' und 'Wie sieht ein Salzkristall aus?' ergeben 'crystal:salt', 'Zeig mir Diamant in 3D' ergibt 'crystal:diamond'). "
    "Bei 'Was ist ein Diamant?' oder 'Wie sieht ein Diamant aus?' bleibt model leer.",
    true, crystalFetch, crystalUnload, crystalBegin, crystalFrame, crystalEnd, nullptr, nullptr, nullptr};
