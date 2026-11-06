// Molecules: real 3D data from PubChem, atoms and bonds. See edu_feature.h.

static const float MODEL_ZOOMS[3] = {1.0f, 1.5f, 2.2f};
static const float MODEL_CAMERA = 3.0f;  // Distance from the model's centre, in model radii
static const int MODEL_LEGEND_MAX = 8;
static const unsigned int MODEL_LABEL_LIMIT = 100;
static const size_t MODEL_BODY_LIMIT = 64 * 1024;

static float* modelZ = nullptr;
static int16_t* modelSX = nullptr;
static int16_t* modelSY = nullptr;
static uint8_t* modelR = nullptr;
static uint8_t* modelOrder = nullptr;
static uint16_t* modelAdjStart = nullptr;  // Bonds per atom: adjacency in compact form
static uint8_t* modelAdj = nullptr;
static uint8_t* modelAdjOrder = nullptr;
static uint8_t modelLegend[MODEL_LEGEND_MAX];
static uint8_t modelLegendCount = 0;
static int modelLegendWidth = 0;

static M5Canvas* moleculeCanvas = nullptr;

static void moleculeEnd() {
  free(modelZ); modelZ = nullptr;
  free(modelSX); modelSX = nullptr;
  free(modelSY); modelSY = nullptr;
  free(modelR); modelR = nullptr;
  free(modelOrder); modelOrder = nullptr;
  free(modelAdjStart); modelAdjStart = nullptr;
  free(modelAdj); modelAdj = nullptr;
  free(modelAdjOrder); modelAdjOrder = nullptr;
}

static bool modelBuildAdjacency() {
  const ModelMolecule& m = *currentMolecule;
  memset(modelAdjStart, 0, sizeof(uint16_t) * (m.atomCount + 1));
  for (uint16_t i = 0; i < m.bondCount; i++) {
    modelAdjStart[m.bonds[i].a + 1]++;
    modelAdjStart[m.bonds[i].b + 1]++;
  }
  for (uint16_t i = 0; i < m.atomCount; i++) modelAdjStart[i + 1] += modelAdjStart[i];
  uint16_t* fill = (uint16_t*)malloc(sizeof(uint16_t) * m.atomCount);
  if (!fill) return false;
  memcpy(fill, modelAdjStart, sizeof(uint16_t) * m.atomCount);
  for (uint16_t i = 0; i < m.bondCount; i++) {
    const ModelBond& b = m.bonds[i];
    modelAdj[fill[b.a]] = b.b;
    modelAdjOrder[fill[b.a]++] = b.order;
    modelAdj[fill[b.b]] = b.a;
    modelAdjOrder[fill[b.b]++] = b.order;
  }
  free(fill);
  return true;
}

static void modelLegendAdd(uint8_t element) {
  for (uint8_t i = 0; i < modelLegendCount; i++) {
    if (modelLegend[i] == element) return;
  }
  if (modelLegendCount < MODEL_LEGEND_MAX) modelLegend[modelLegendCount++] = element;
}

static void modelBuildLegend(M5Canvas& c) {
  const ModelMolecule& m = *currentMolecule;
  modelLegendCount = 0;
  // Carbon and hydrogen first, as chemists write formulas; then in order of appearance
  for (uint16_t i = 0; i < m.atomCount; i++) {
    if (m.atoms[i].element == 6) { modelLegendAdd(6); break; }
  }
  for (uint16_t i = 0; i < m.atomCount; i++) {
    if (m.atoms[i].element == 1) { modelLegendAdd(1); break; }
  }
  for (uint16_t i = 0; i < m.atomCount; i++) modelLegendAdd(m.atoms[i].element);
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  modelLegendWidth = 0;
  for (uint8_t i = 0; i < modelLegendCount; i++) modelLegendWidth += c.textWidth(elementSymbol(modelLegend[i])) + 3;
}

static bool moleculeBegin(M5Canvas& c) {
  if (!currentMolecule) return false;
  const uint16_t atoms = currentMolecule->atomCount;
  const uint16_t half = currentMolecule->bondCount * 2;
  modelZ = (float*)malloc(sizeof(float) * atoms);
  modelSX = (int16_t*)malloc(sizeof(int16_t) * atoms);
  modelSY = (int16_t*)malloc(sizeof(int16_t) * atoms);
  modelR = (uint8_t*)malloc(atoms);
  modelOrder = (uint8_t*)malloc(atoms);
  modelAdjStart = (uint16_t*)malloc(sizeof(uint16_t) * (atoms + 1));
  modelAdj = (uint8_t*)malloc(half + 1);
  modelAdjOrder = (uint8_t*)malloc(half + 1);
  if (!modelZ || !modelSX || !modelSY || !modelR || !modelOrder || !modelAdjStart || !modelAdj || !modelAdjOrder ||
      !modelBuildAdjacency()) {
    moleculeEnd();
    return false;
  }
  for (uint16_t i = 0; i < atoms; i++) modelOrder[i] = (uint8_t)i;
  modelBuildLegend(c);
  return true;
}

static uint8_t modelAtomRadius(uint8_t element) {
  if (element == 1) return 3;   // Hydrogen
  if (element <= 10) return 5;  // C, N, O ...
  return 7;                     // Larger atoms
}

// One half of a bond, from the atom to the middle, in that atom's colour
static void modelDrawHalfBond(int x0, int y0, int x1, int y1, uint8_t order, uint16_t color) {
  M5Canvas& c = *moleculeCanvas;
  int dx = x1 - x0, dy = y1 - y0;
  // Step sideways along the larger axis so parallel lines stay parallel on the pixel grid
  bool steep = abs(dy) > abs(dx);
  int sx = steep ? 1 : 0, sy = steep ? 0 : 1;
  if (order >= 2) {
    c.drawLine(x0 - 2 * sx, y0 - 2 * sy, x1 - 2 * sx, y1 - 2 * sy, color);
    c.drawLine(x0 - sx, y0 - sy, x1 - sx, y1 - sy, color);
    c.drawLine(x0 + 2 * sx, y0 + 2 * sy, x1 + 2 * sx, y1 + 2 * sy, color);
    c.drawLine(x0 + sx, y0 + sy, x1 + sx, y1 + sy, color);
    if (order >= 3) c.drawLine(x0, y0, x1, y1, color);
  } else {
    c.drawLine(x0, y0, x1, y1, color);
    c.drawLine(x0 + sx, y0 + sy, x1 + sx, y1 + sy, color);
  }
}

static void moleculeFrame(M5Canvas& c, float dt) {
  (void)dt;
  moleculeCanvas = &c;
  const ModelMolecule& m = *currentMolecule;
  c.fillSprite(TFT_BLACK);
  const int cx = WIDTH / 2;
  const int cy = (HEIGHT - EDU_FOOTER_HEIGHT) / 2;
  const float zoom = MODEL_ZOOMS[modelZoomStep];
  const float scale = (HEIGHT - EDU_FOOTER_HEIGHT) * 0.42f * zoom;

  float p[3];
  for (uint16_t i = 0; i < m.atomCount; i++) {
    const ModelAtom& a = m.atoms[i];
    rotatePoint(modelRotation, a.x, a.y, a.z, p);
    const float perspective = MODEL_CAMERA / (MODEL_CAMERA - p[2]);
    modelSX[i] = (int16_t)(cx + p[0] * scale * perspective);
    modelSY[i] = (int16_t)(cy - p[1] * scale * perspective);
    modelZ[i] = p[2];
    int r = (int)(modelAtomRadius(a.element) * zoom * perspective + 0.5f);
    modelR[i] = (uint8_t)(r < 1 ? 1 : r);
  }
  sortByDepth(modelZ, modelOrder, m.atomCount);

  for (uint16_t n = 0; n < m.atomCount; n++) {
    const uint8_t i = modelOrder[n];
    const uint16_t color = elementColor565(m.atoms[i].element);
    const int x = modelSX[i], y = modelSY[i], r = modelR[i];
    for (uint16_t k = modelAdjStart[i]; k < modelAdjStart[i + 1]; k++) {
      const uint8_t other = modelAdj[k];
      modelDrawHalfBond(x, y, (x + modelSX[other]) / 2, (y + modelSY[other]) / 2, modelAdjOrder[k], color);
    }
    // A dark rim keeps overlapping atoms apart, which is what makes the depth readable
    c.fillCircle(x, y, r + 1, TFT_BLACK);
    c.fillCircle(x, y, r, color);
    // Only on larger atoms: a highlight on a small one reads as a + sign, which means charge
    if (r >= 6) c.fillRect(x - r / 3 - 1, y - r / 3 - 1, 2, 2, TFT_WHITE);
  }

  eduDrawFooter(c, modelLegendWidth);
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  int x = WIDTH - 2;
  for (int i = modelLegendCount - 1; i >= 0; i--) {
    const char* symbol = elementSymbol(modelLegend[i]);
    x -= c.textWidth(symbol);
    c.setTextColor(elementColor565(modelLegend[i]));
    c.drawString(symbol, x, HEIGHT - 4);
    x -= 3;
  }
}

// Fills currentMolecule and modelLabel from PubChem. False on anything unexpected.
static bool moleculeFetch(const char* name) {
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  char* body = (char*)ps_malloc(MODEL_BODY_LIMIT);
  if (!body) return false;
  if (!currentMolecule) currentMolecule = (ModelMolecule*)malloc(sizeof(ModelMolecule));
  if (!currentMolecule) {
    free(body);
    return false;
  }
  const String base = "https://" + modelPubChemHost + "/rest/pug/compound/name/" + eduUrlEncode(name);
  size_t length = 0;
  bool ok = eduHttpGet(base + "/SDF?record_type=3d", deadline, body, MODEL_BODY_LIMIT, length) &&
            parseSdf(body, length, *currentMolecule);
  if (ok) {
    // The label comes from the record, so a wrong lookup is visible on screen
    ok = eduHttpGet(base + "/property/Title/JSON", deadline, body, MODEL_BODY_LIMIT, length);
    if (ok) {
      JsonDocument doc;
      ok = !deserializeJson(doc, body, length);
      const char* title = ok ? (doc["PropertyTable"]["Properties"][0]["Title"] | "") : "";
      ok = ok && title[0];
      if (ok) {
        // A label fits one line; a longer one can only come from a broken or hostile server
        modelLabel = title;
        if (modelLabel.length() > MODEL_LABEL_LIMIT) {
          int cut = MODEL_LABEL_LIMIT;
          while (cut > 0 && ((uint8_t)modelLabel[cut] & 0xC0) == 0x80) cut--;
          modelLabel.remove(cut);
        }
      }
    }
  }
  free(body);
  Serial.printf("Model lookup '%s': %s, %u atoms, %ld ms left\n", name, ok ? "found" : "not available",
                ok ? currentMolecule->atomCount : 0, (long)(deadline - millis()));
  return ok;
}

const EduFeature FEATURE_MOLECULE = {
    "molecule", "PubChem",
    nullptr,  // The molecule rule is part of the main prompt
    true, moleculeFetch, nullptr, moleculeBegin, moleculeFrame, moleculeEnd, nullptr, nullptr, nullptr};

// Serial "modelbench <atoms>": frames per second for a made-up test molecule.
// Only measures drawing speed; this shape is never shown as a real model.
void modelBench(int atoms) {
  if (atoms < 2) atoms = 2;
  if (atoms > MODEL_MAX_ATOMS) atoms = MODEL_MAX_ATOMS;
  ModelMolecule* bench = (ModelMolecule*)malloc(sizeof(ModelMolecule));
  if (!bench) {
    Serial.println("modelbench: no memory");
    return;
  }
  bench->atomCount = (uint16_t)atoms;
  bench->bondCount = (uint16_t)min(atoms - 1 + atoms / 4, MODEL_MAX_BONDS);
  const uint8_t elements[4] = {6, 1, 8, 7};
  for (int i = 0; i < atoms; i++) {
    const float t = (float)i / (atoms - 1);
    const float angle = t * 40.0f;
    const float radius = sqrtf(1.0f - (2 * t - 1) * (2 * t - 1));
    bench->atoms[i] = {radius * cosf(angle), 2 * t - 1, radius * sinf(angle), elements[i % 4]};
  }
  for (int i = 0; i < bench->bondCount; i++) {
    const int a = i < atoms - 1 ? i : (i - (atoms - 1)) * 3 % atoms;
    const int b = i < atoms - 1 ? i + 1 : (a + 2) % atoms;
    bench->bonds[i] = {(uint8_t)a, (uint8_t)b, (uint8_t)(i % 5 == 0 ? 2 : 1)};
  }
  ModelMolecule* saved = currentMolecule;
  const EduFeature* savedFeature = activeFeature;
  activeFeature = &FEATURE_MOLECULE;
  currentMolecule = bench;
  const String savedLabel = modelLabel;
  modelLabel = "Benchmark";
  if (modelViewBegin()) {
    mat3Identity(modelRotation);
    for (uint8_t zoom = 0; zoom < 3; zoom++) {
      modelZoomStep = zoom;
      const unsigned long start = millis();
      int frames = 0;
      while (millis() - start < 3000) {
        mat3Rotate(modelRotation, 0.01f, 0.03f, 0.0f);
        modelViewRender(0.03f);
        frames++;
        yield();
      }
      Serial.printf("modelbench atoms=%d bonds=%d zoom=%d fps=%.1f sprite=%s freeHeap=%u\n", atoms, bench->bondCount,
                    zoom, frames * 1000.0f / (millis() - start), modelViewSpriteInPsram() ? "psram" : "internal",
                    (unsigned)ESP.getFreeHeap());
    }
    modelZoomStep = 0;
    modelViewEnd();
  } else {
    Serial.println("modelbench: no memory for the sprite");
  }
  modelLabel = savedLabel;
  activeFeature = savedFeature;
  currentMolecule = saved;
  free(bench);
}
